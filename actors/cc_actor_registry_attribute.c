/*
 * Copyright (c) 2016 Ericsson AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdlib.h>
#include <string.h>
#include "cc_actor_button.h"
#include "../runtime/north/cc_actor.h"
#include "../runtime/north/cc_actor_store.h"
#include "../runtime/north/cc_port.h"
#include "../runtime/north/cc_token.h"
#include "../runtime/north/cc_msgpack_helper.h"
#include "../msgpuck/msgpuck.h"
#include "../calvinsys/cc_calvinsys.h"

static result_t actor_registry_attribute_init(actor_t **actor, list_t *attributes)
{
  calvinsys_obj_t *obj = NULL;
  list_t *tmp = attributes;
  char *data = NULL;
  uint32_t data_len = 0;

  while (tmp != NULL) {
    if (strncmp(tmp->id, "attr", 4) == 0) {
      data = (char *)tmp->data;
      data_len = tmp->data_len;
      break;
    }
    tmp = tmp->next;
  }

  if (data == NULL) {
    cc_log_error("Failed to get attribute 'attribute'");
    return CC_RESULT_FAIL;
  }

	obj = calvinsys_open((*actor)->calvinsys, "sys.attribute.indexed", NULL, 0);
	if (obj == NULL) {
		cc_log_error("Failed to open 'sys.attribute.indexed'");
		return CC_RESULT_FAIL;
	}

  obj->write(obj, data, data_len);

	(*actor)->instance_state = (void *)obj;

	return CC_RESULT_SUCCESS;
}

static result_t actor_registry_attribute_set_state(actor_t **actor, list_t *attributes)
{
	return actor_registry_attribute_init(actor, attributes);
}

static bool actor_registry_attribute_fire(struct actor_t *actor)
{
	port_t *inport = (port_t *)actor->in_ports->data, *outport = (port_t *)actor->out_ports->data;
  calvinsys_obj_t *obj = (calvinsys_obj_t *)actor->instance_state;
  char *data = NULL;
  size_t size = 0;

	if (obj->can_read(obj) && fifo_tokens_available(inport->fifo, 1) && fifo_slots_available(outport->fifo, 1)) {
		if (obj->read(obj, &data, &size) == CC_RESULT_SUCCESS) {
      fifo_peek(inport->fifo);
      if (fifo_write(outport->fifo, data, size) == CC_RESULT_SUCCESS) {
        fifo_commit_read(inport->fifo, true);
        return true;
      }
      cc_log_error("Could not write to ouport");
      fifo_cancel_commit(inport->fifo);
    } else
      cc_log_error("Failed to from calvinsys object");
	}
	return false;
}

static void actor_registry_attribute_free(actor_t *actor)
{
	calvinsys_close((calvinsys_obj_t *)actor->instance_state);
}

result_t actor_registry_attribute_register(list_t **actor_types)
{
	actor_type_t *type = NULL;

	if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	type->init = actor_registry_attribute_init;
	type->set_state = actor_registry_attribute_set_state;
	type->free_state = actor_registry_attribute_free;
	type->fire_actor = actor_registry_attribute_fire;
	type->get_managed_attributes = NULL;
	type->will_migrate = NULL;
	type->will_end = NULL;
	type->did_migrate = NULL;

	return list_add_n(actor_types, "context.RegistryAttribute", 25, type, sizeof(actor_type_t *));
}
