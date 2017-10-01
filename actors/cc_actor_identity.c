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
//#include <stdlib.h>
#include <string.h>
#include "cc_actor_identity.h"
#include "../runtime/north/cc_actor_store.h"
#include "../runtime/north/cc_port.h"
#include "../runtime/north/cc_token.h"
#include "../runtime/north/coder/cc_coder.h"
#include "../runtime/south/platform/cc_platform.h"

typedef struct state_identity_t {
	bool dump;
} state_identity_t;

static cc_result_t cc_actor_identity_init(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	state_identity_t *state = NULL;
	bool dump = false;
	cc_list_t *item = NULL;

	item = cc_list_get(managed_attributes, "dump");
	if (item == NULL) {
		cc_log_error("Failed to get attribute 'dump'");
		return CC_FAIL;
	}

	if (cc_coder_decode_bool((char *)item->data, &dump) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_platform_mem_alloc((void **)&state, sizeof(state_identity_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	state->dump = dump;
	(*actor)->instance_state = (void *)state;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_identity_set_state(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	return cc_actor_identity_init(actor, managed_attributes);
}

static bool cc_actor_identity_fire(struct cc_actor_t *actor)
{
	cc_token_t *token = NULL;
	cc_port_t *inport = (cc_port_t *)actor->in_ports->data; // only 1 inport
	cc_port_t *outport = (cc_port_t *)actor->out_ports->data; // only 1 outport

	if (!cc_fifo_tokens_available(inport->fifo, 1))
	 	return false;

	if (!cc_fifo_slots_available(outport->fifo, 1))
		return false;

	token = cc_fifo_peek(inport->fifo);
	if (cc_fifo_write(outport->fifo, token->value, token->size) != CC_SUCCESS) {
		cc_fifo_cancel_commit(inport->fifo);
		return false;
	}

	cc_fifo_commit_read(inport->fifo, false);

	return true;
}

static void cc_actor_identity_free(cc_actor_t *actor)
{
	if (actor->instance_state)
		cc_platform_mem_free(actor->instance_state);
}

static cc_result_t cc_actor_identity_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	uint32_t size = 0;
	char *dump = NULL, *last = NULL;
	state_identity_t *state = (state_identity_t *)actor->instance_state;

	size = cc_coder_sizeof_bool(state->dump);
	if (cc_platform_mem_alloc((void **)&dump, size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	cc_coder_encode_bool(dump, state->dump);

	if (cc_list_add_n(managed_attributes, "dump", 4, dump, size) == NULL) {
		cc_log_error("Failed to add 'dump' to managed list");
		cc_platform_mem_free(dump);
		return CC_FAIL;
	}

	size = cc_coder_sizeof_nil();
	if (cc_platform_mem_alloc((void **)&last, size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	cc_coder_encode_nil(last);

	if (cc_list_add_n(managed_attributes, "last", 4, last, size) == NULL) {
		cc_log_error("Failed to add 'last' to managed list");
		cc_platform_mem_free(last);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_identity_register(cc_list_t **actor_types)
{
	cc_actor_type_t *type = NULL;

	if (cc_platform_mem_alloc((void **)&type, sizeof(cc_actor_type_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(type, 0, sizeof(cc_actor_type_t));
	type->init = cc_actor_identity_init;
	type->set_state = cc_actor_identity_set_state;
	type->free_state = cc_actor_identity_free;
	type->fire_actor = cc_actor_identity_fire;
	type->get_managed_attributes = cc_actor_identity_get_attributes;

	if (cc_list_add_n(actor_types, "std.Identity", 12, type, sizeof(cc_actor_type_t *)) == NULL)
		return CC_FAIL;

	return CC_SUCCESS;
}
