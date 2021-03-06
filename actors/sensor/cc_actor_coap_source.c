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
#include "runtime/north/cc_actor_store.h"
#include "runtime/north/cc_fifo.h"
#include "runtime/north/cc_token.h"
#include "runtime/north/cc_port.h"
#include "runtime/north/cc_common.h"
#include "runtime/north/coder/cc_coder.h"
#include "calvinsys/cc_calvinsys.h"

typedef struct cc_actor_coap_state_t {
	char source[CC_UUID_BUFFER_SIZE];
} cc_actor_coap_state_t;

static cc_result_t cc_actor_coap_init(cc_actor_t *actor, cc_list_t *managed_attributes)
{
  cc_actor_coap_state_t *state = NULL;
  char *obj_ref = NULL;

  if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_coap_state_t)) != CC_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return CC_FAIL;
  }

  memset(state, 0, sizeof(cc_actor_coap_state_t));
  actor->instance_state = (void *)state;

  obj_ref = cc_calvinsys_open(actor, actor->requires, NULL);
  if (obj_ref == NULL) {
    cc_log_error("Failed to open '%s'", actor->requires);
    return CC_FAIL;
  }
  strncpy(state->source, obj_ref, strlen(obj_ref));
  state->source[strlen(obj_ref)] = '\0';

  if (cc_calvinsys_write(actor->calvinsys, state->source, "\xC3", 1) != CC_SUCCESS) {
    cc_log_error("Failed to init '%s'", actor->requires);
    return false;
  }

  return CC_SUCCESS;
}

static cc_result_t cc_actor_coap_set_state(cc_actor_t *actor, cc_list_t *managed_attributes)
{
	cc_actor_coap_state_t *state = NULL;
	cc_list_t *item = NULL;
	char *obj_ref = NULL, *name = NULL;
	uint32_t obj_ref_len = 0;

  name = actor->requires;
  if ((name = strchr(name, '.')) == NULL) {
    cc_log_error("Failed to parse capability name from %s (expected '.')", actor->requires);
    return CC_FAIL;
  }
  name++;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_coap_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(state, 0, sizeof(cc_actor_coap_state_t));

	item = cc_list_get(managed_attributes, name);
	if (item == NULL) {
		cc_log_error("Failed to get capability '%s' from managed list", name);
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	if (cc_coder_decode_str((char *)item->data, &obj_ref, &obj_ref_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode '%s'", name);
		cc_platform_mem_free(state);
		return CC_FAIL;
	}
	strncpy(state->source, obj_ref, obj_ref_len);
	state->source[obj_ref_len] = '\0';
	actor->instance_state = (void *)state;

	if (cc_calvinsys_write(actor->calvinsys, state->source, "\xC3", 1) != CC_SUCCESS) {
		cc_log_error("Failed to init '%s'", actor->requires);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static bool cc_actor_coap_read_measurement(struct cc_actor_t *actor)
{
	cc_actor_coap_state_t *state = (cc_actor_coap_state_t *)actor->instance_state;
	cc_port_t *port = (cc_port_t *)actor->out_ports->data;
	char *data = NULL;
	size_t size = 0;

	if (!cc_fifo_slots_available(port->fifo, 1))
		return false;

	if (!cc_calvinsys_can_read(actor->calvinsys, state->source))
		return false;

	if (cc_calvinsys_read(actor->calvinsys, state->source, &data, &size) != CC_SUCCESS)
		return false;

	if (cc_fifo_write(port->fifo, data, size) != CC_SUCCESS) {
		cc_platform_mem_free((void *)data);
		return false;
	}

	return true;
}

static bool cc_actor_coap_fire(struct cc_actor_t *actor)
{
	return cc_actor_coap_read_measurement(actor);
}

static void cc_actor_coap_free(cc_actor_t *actor)
{
	if (actor->instance_state != NULL)
		cc_platform_mem_free((void *)actor->instance_state);
}

static cc_result_t cc_actor_coap_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	cc_actor_coap_state_t *state = (cc_actor_coap_state_t *)actor->instance_state;
	char *buffer = NULL, *w = NULL, *name = NULL;

  name = actor->requires;
  if ((name = strchr(name, '.')) == NULL) {
    cc_log_error("Failed to parse capability name from %s (expected '.')", actor->requires);
    return CC_FAIL;
  }
  name++;

	if (cc_platform_mem_alloc((void **)&buffer, cc_coder_sizeof_str(strlen(state->source))) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	w = buffer;
	w = cc_coder_encode_str(w, state->source, strlen(state->source));

	if (cc_list_add_n(managed_attributes, name, strlen(name), buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add '%s' to managed attributes", name);
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_coap_get_requires(cc_actor_t *actor, cc_list_t **requires)
{
  if (cc_list_add_n(requires, actor->requires, strlen(actor->requires), NULL, 0) == NULL) {
    cc_log_error("Failed to add '%s'", actor->requires);
    return CC_FAIL;
  }

	return CC_SUCCESS;
}

cc_result_t cc_actor_coap_setup(cc_actor_type_t *type)
{
	type->init = cc_actor_coap_init;
	type->set_state = cc_actor_coap_set_state;
	type->free_state = cc_actor_coap_free;
	type->fire_actor = cc_actor_coap_fire;
	type->get_requires = cc_actor_coap_get_requires;
	type->get_managed_attributes = cc_actor_coap_get_attributes;

	return CC_SUCCESS;
}
