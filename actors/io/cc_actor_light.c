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
#include "runtime/north/coder/cc_coder.h"
#include "runtime/north/cc_fifo.h"

typedef struct cc_actor_light_state_t {
	char light[CC_UUID_BUFFER_SIZE];
} cc_actor_light_state_t;

static cc_result_t cc_actor_light_init(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	char *obj_ref = NULL;
	cc_actor_light_state_t *state = NULL;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_light_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(state, 0, sizeof(cc_actor_light_state_t));
	(*actor)->instance_state = (void *)state;

	obj_ref = cc_calvinsys_open(*actor, "io.light", NULL);
	if (obj_ref == NULL) {
		cc_log_error("Failed to open 'io.light'");
		return CC_FAIL;
	}

	strncpy(state->light, obj_ref, strlen(obj_ref));
	state->light[strlen(obj_ref)] = '\0';

	return CC_SUCCESS;
}

static cc_result_t cc_actor_light_set_state(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	cc_list_t *item = NULL;
	cc_actor_light_state_t *state = NULL;
	char *obj_ref = NULL;
	uint32_t obj_ref_len = 0;

	item = cc_list_get(managed_attributes, "light");
	if (item == NULL) {
		cc_log_error("Failed to get 'light'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_light_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(state, 0, sizeof(cc_actor_light_state_t));
	(*actor)->instance_state = state;

	if (cc_coder_decode_str((char *)item->data, &obj_ref, &obj_ref_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'light'");
		return CC_FAIL;
	}
	strncpy(state->light, obj_ref, obj_ref_len);
	state->light[obj_ref_len] = '\0';

	return CC_SUCCESS;
}

static bool cc_actor_light_fire(struct cc_actor_t *actor)
{
	cc_port_t *inport = (cc_port_t *)actor->in_ports->data;
	cc_actor_light_state_t *state = (cc_actor_light_state_t *)actor->instance_state;
	cc_token_t *token = NULL;

	if (!cc_fifo_tokens_available(inport->fifo, 1))
		return false;

	token = cc_fifo_peek(inport->fifo);
	if (cc_calvinsys_write(actor->calvinsys, state->light, token->value, token->size) != CC_SUCCESS) {
		cc_fifo_cancel_commit(inport->fifo);
		return false;
	}

	cc_fifo_commit_read(inport->fifo, true);

	return true;
}

static cc_result_t cc_actor_light_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	char *buffer = NULL, *w = NULL;
	uint32_t buffer_len = 0;
	cc_actor_light_state_t *state = (cc_actor_light_state_t *)actor->instance_state;

	buffer_len = cc_coder_sizeof_str(strlen(state->light));

	if (cc_platform_mem_alloc((void **)&buffer, buffer_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	w = buffer;
	w = cc_coder_encode_str(w, state->light, strlen(state->light));

	if (cc_list_add_n(managed_attributes, "light", 5, buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add 'light' to managed attributes");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_light_get_requires(cc_actor_t *actor, cc_list_t **requires)
{
	if (cc_list_add_n(requires, "io.light", 8, NULL, 0) == NULL) {
		cc_log_error("Failed to add requires");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static void cc_actor_light_free(cc_actor_t *actor)
{
	if (actor->instance_state != NULL)
		cc_platform_mem_free(actor->instance_state);
}

cc_result_t cc_actor_light_setup(cc_actor_type_t *type)
{
	type->init = cc_actor_light_init;
	type->set_state = cc_actor_light_set_state;
	type->fire_actor = cc_actor_light_fire;
	type->get_requires = cc_actor_light_get_requires;
	type->get_managed_attributes = cc_actor_light_get_attributes;
	type->free_state = cc_actor_light_free;

	return CC_SUCCESS;
}
