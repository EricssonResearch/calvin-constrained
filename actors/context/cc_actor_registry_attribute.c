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
#include "runtime/north/cc_actor.h"
#include "runtime/north/cc_actor_store.h"
#include "runtime/north/cc_port.h"
#include "runtime/north/cc_token.h"
#include "runtime/north/coder/cc_coder.h"
#include "calvinsys/cc_calvinsys.h"

typedef struct cc_actor_registry_state_t {
	char registry[CC_UUID_BUFFER_SIZE];
} cc_actor_registry_state_t;

static cc_result_t cc_actor_registry_attribute_init(cc_actor_t *actor, cc_list_t *managed_attributes)
{
  cc_list_t *item = NULL;
  char *obj_ref = NULL;
  cc_actor_registry_state_t *state = NULL;

  item = cc_list_get(managed_attributes, "attribute");
  if (item == NULL) {
    cc_log_error("Failed to get 'attribute'");
    return CC_FAIL;
  }

	obj_ref = cc_calvinsys_open(actor, "sys.attribute.indexed", NULL);
	if (obj_ref == NULL) {
		cc_log_error("Failed to open 'sys.attribute.indexed'");
		return CC_FAIL;
	}

  if (cc_calvinsys_write(actor->calvinsys, obj_ref, (char *)item->data, item->data_len) != CC_SUCCESS) {
    cc_log_error("Failed to set attribute");
    return CC_FAIL;
  }

  if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_registry_state_t)) != CC_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return CC_FAIL;
  }
  strcpy(state->registry, obj_ref);

	actor->instance_state = (void *)state;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_registry_attribute_set_state(cc_actor_t *actor, cc_list_t *managed_attributes)
{
  cc_list_t *item = NULL;
  char *obj_ref = NULL;
  uint32_t obj_ref_len = 0;
  cc_actor_registry_state_t *state = NULL;

  item = cc_list_get(managed_attributes, "registry");
  if (item == NULL) {
    cc_log_error("Failed to get 'registry'");
    return CC_FAIL;
  }

  if (cc_coder_decode_str((char *)item->data, &obj_ref, &obj_ref_len) != CC_SUCCESS) {
    cc_log_error("Failed to decode 'registry'");
    return CC_FAIL;
  }

  if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_registry_state_t)) != CC_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return CC_FAIL;
  }

  strncpy(state->registry, obj_ref, obj_ref_len);
  state->registry[obj_ref_len] = '\0';

  actor->instance_state = (void *)state->registry;

  return CC_SUCCESS;
}

static bool cc_actor_registry_attribute_fire(struct cc_actor_t*actor)
{
	cc_port_t *inport = (cc_port_t *)actor->in_ports->data;
  cc_port_t *outport = (cc_port_t *)actor->out_ports->data;
  char *obj_ref = (char *)actor->instance_state;
  char *data = NULL;
  size_t size = 0;
  cc_actor_registry_state_t *state = NULL;

  if (actor->instance_state == NULL) {
    cc_log_error("Actor does not have a state");
    return CC_FAIL;
  }

  state = (cc_actor_registry_state_t *)actor->instance_state;

	if (!cc_calvinsys_can_read(actor->calvinsys, state->registry))
    return false;

  if (!cc_fifo_tokens_available(inport->fifo, 1))
    return false;

  if (!cc_fifo_slots_available(outport->fifo, 1))
    return false;

  if (cc_calvinsys_read(actor->calvinsys, obj_ref, &data, &size) != CC_SUCCESS)
    return false;

  cc_fifo_peek(inport->fifo);
  if (cc_fifo_write(outport->fifo, data, size) != CC_SUCCESS) {
    cc_fifo_cancel_commit(inport->fifo);
    return false;
  }
  cc_fifo_commit_read(inport->fifo, true);

  return true;
}

static cc_result_t cc_actor_registry_attribute_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	char *buffer = NULL, *w = NULL;
  cc_actor_registry_state_t *state = NULL;

  if (actor->instance_state == NULL) {
    cc_log_error("Actor does not have a state");
    return CC_FAIL;
  }

  state = (cc_actor_registry_state_t *)actor->instance_state;

	if (cc_platform_mem_alloc((void **)&buffer, cc_coder_sizeof_str(strlen(state->registry))) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

  w = buffer;
	w = cc_coder_encode_str(w, state->registry, strlen(state->registry));

	if (cc_list_add_n(managed_attributes, "registry", 8, buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add 'registry' to managed attributes");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static void cc_actor_registry_attribute_free(cc_actor_t *actor)
{
	if (actor->instance_state != NULL)
		cc_platform_mem_free((void *)actor->instance_state);
}

cc_result_t cc_actor_registry_attribute_get_requires(cc_actor_t *actor, cc_list_t **requires)
{
	if (cc_list_add_n(requires, "sys.attribute.indexed", 21, NULL, 0) == NULL) {
		cc_log_error("Failed to add requires");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_registry_attribute_setup(cc_actor_type_t *type)
{
	type->init = cc_actor_registry_attribute_init;
	type->set_state = cc_actor_registry_attribute_set_state;
	type->fire_actor = cc_actor_registry_attribute_fire;
  type->get_managed_attributes = cc_actor_registry_attribute_get_attributes;
  type->free_state = cc_actor_registry_attribute_free;
	type->get_requires = cc_actor_registry_attribute_get_requires;

  return CC_SUCCESS;
}
