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
#include "cc_actor_light.h"
#include "../runtime/north/cc_actor_store.h"
#include "../runtime/north/cc_msgpack_helper.h"
#include "../runtime/north/cc_fifo.h"
#include "../msgpuck/msgpuck.h"

static result_t actor_light_init(actor_t **actor, list_t *attributes)
{
	calvinsys_obj_t *obj = NULL;

	obj = calvinsys_open((*actor)->calvinsys, "io.light", NULL, 0);
	if (obj == NULL) {
		cc_log_error("Failed to open 'io.light'");
		return CC_RESULT_FAIL;
	}

	(*actor)->instance_state = (void *)obj;

	return CC_RESULT_SUCCESS;
}

static result_t actor_light_set_state(actor_t **actor, list_t *attributes)
{
	return actor_light_init(actor, attributes);
}

static bool actor_light_fire(struct actor_t *actor)
{
	port_t *inport = (port_t *)actor->in_ports->data;
	calvinsys_obj_t *obj = (calvinsys_obj_t *)actor->instance_state;
	token_t *token = NULL;

	if (fifo_tokens_available(inport->fifo, 1)) {
		token = fifo_peek(inport->fifo);
		if (obj->write(obj, token->value, token->size) == CC_RESULT_SUCCESS) {
			fifo_commit_read(inport->fifo, true);
			return true;
		}
		fifo_cancel_commit(inport->fifo);
	}

	return false;
}

static void actor_light_free(actor_t *actor)
{
	calvinsys_close((calvinsys_obj_t *)actor->instance_state);
}

result_t actor_light_register(list_t **actor_types)
{
	actor_type_t *type = NULL;

	if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	type->init = actor_light_init;
	type->set_state = actor_light_set_state;
	type->free_state = actor_light_free;
	type->fire_actor = actor_light_fire;
	type->get_managed_attributes = NULL;
	type->will_migrate = NULL;
	type->will_end = NULL;
	type->did_migrate = NULL;

	return list_add_n(actor_types, "io.Light", 8, type, sizeof(actor_type_t *));
}
