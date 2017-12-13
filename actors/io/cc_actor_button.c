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

typedef struct cc_actor_button_state_t {
	char button[CC_UUID_BUFFER_SIZE];
	char *text;
	uint32_t text_len;
} cc_actor_button_state_t;

static cc_result_t cc_actor_button_init(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	char *obj_ref = NULL;
	cc_actor_button_state_t *state = NULL;
	cc_list_t *item = NULL;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_button_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(state, 0, sizeof(cc_actor_button_state_t));
	(*actor)->instance_state = (void *)state;

	obj_ref = cc_calvinsys_open(*actor, "io.button", NULL);
	if (obj_ref == NULL) {
		cc_log_error("Failed to open 'io.button'");
		return CC_FAIL;
	}
	strncpy(state->button, obj_ref, strlen(obj_ref));
	state->button[strlen(obj_ref)] = '\0';

	item = cc_list_get(managed_attributes, "text");
	if (item != NULL) {
		if (cc_platform_mem_alloc((void **)&state->text, item->data_len) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		memcpy(state->text, item->data, item->data_len);
		state->text_len = item->data_len;
	}

	return CC_SUCCESS;
}

static cc_result_t cc_actor_button_set_state(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	cc_list_t *item = NULL;
	cc_actor_button_state_t *state = NULL;
	char *obj_ref = NULL;
	uint32_t obj_ref_len = 0;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_button_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(state, 0, sizeof(cc_actor_button_state_t));
	(*actor)->instance_state = (void *)state;

	item = cc_list_get(managed_attributes, "button");
	if (item == NULL) {
		cc_log_error("Failed to get 'button'");
		return CC_FAIL;
	}

	if (cc_coder_decode_str((char *)item->data, &obj_ref, &obj_ref_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'button'");
		return CC_FAIL;
	}
	strncpy(state->button, obj_ref, obj_ref_len);
	state->button[obj_ref_len] = '\0';

	item = cc_list_get(managed_attributes, "text");
	if (item == NULL) {
		cc_log_error("Failed to get 'text'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state->text, item->data_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	memcpy(state->text, item->data, item->data_len);
	state->text_len = item->data_len;

	return CC_SUCCESS;
}

static bool cc_actor_button_fire(struct cc_actor_t *actor)
{
	cc_port_t *outport = (cc_port_t *)actor->out_ports->data;
	char *data = NULL;
	size_t size = 0;
	cc_actor_button_state_t *state = (cc_actor_button_state_t *)actor->instance_state;

	if (!cc_calvinsys_can_read(actor->calvinsys, state->button))
		return false;

	if (!cc_fifo_slots_available(outport->fifo, 1))
		return false;

	if (cc_calvinsys_read(actor->calvinsys, state->button, &data, &size) != CC_SUCCESS)
		return false;

	if (cc_fifo_write(outport->fifo, data, size) != CC_SUCCESS) {
		cc_platform_mem_free((void *)data);
		return false;
	}

	return true;
}

static cc_result_t cc_actor_button_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	char *buffer = NULL, *w = NULL;
	uint32_t buffer_len = 0;
	cc_actor_button_state_t *state = (cc_actor_button_state_t *)actor->instance_state;

	buffer_len = cc_coder_sizeof_str(strlen(state->button));

	if (cc_platform_mem_alloc((void **)&buffer, buffer_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	w = buffer;
	w = cc_coder_encode_str(w, state->button, strlen(state->button));

	if (cc_list_add_n(managed_attributes, "button", 6, buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add 'button' to managed attributes");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	if (cc_list_add_n(managed_attributes, "text", 4, state->text, state->text_len) == NULL) {
		cc_log_error("Failed to add 'button' to managed attributes");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}
	state->text = NULL;
	state->text_len = 0;

	return CC_SUCCESS;
}

cc_result_t cc_actor_button_get_requires(cc_actor_t *actor, cc_list_t **requires)
{
	if (cc_list_add_n(requires, "io.button", 9, NULL, 0) == NULL) {
		cc_log_error("Failed to add requires");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static void cc_actor_button_free(cc_actor_t *actor)
{
	cc_actor_button_state_t *state = NULL;

	if (actor->instance_state != NULL) {
		state = (cc_actor_button_state_t *)actor->instance_state;
		if (state->text != NULL)
			cc_platform_mem_free(state->text);
		cc_platform_mem_free(state);
		actor->instance_state = NULL;
	}
}

cc_result_t cc_actor_button_setup(cc_actor_type_t *type)
{
	type->init = cc_actor_button_init;
	type->set_state = cc_actor_button_set_state;
	type->fire_actor = cc_actor_button_fire;
	type->get_managed_attributes = cc_actor_button_get_attributes;
	type->get_requires = cc_actor_button_get_requires;
	type->free_state = cc_actor_button_free;

	return CC_SUCCESS;
}
