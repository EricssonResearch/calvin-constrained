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
#include <stdio.h>
#include <string.h>
#include "runtime/north/cc_actor_store.h"
#include "runtime/north/cc_port.h"
#include "runtime/north/cc_token.h"
#include "runtime/north/coder/cc_coder.h"
#include "runtime/south/platform/cc_platform.h"

typedef struct cc_actor_replica_identity_state_t {
	bool dump;
	uint32_t index;
} cc_actor_replica_identity_state_t;

static cc_result_t cc_actor_replica_identity_init(cc_actor_t *actor, cc_list_t *managed_attributes)
{
	cc_actor_replica_identity_state_t *state = NULL;
	bool dump = false;
	uint32_t index = 0;
	cc_list_t *item = NULL;

	item = cc_list_get(managed_attributes, "dump");
	// If dump missing use default false, shadow args might miss optional args
	if (item != NULL && cc_coder_decode_bool((char *)item->data, &dump) != CC_SUCCESS)
		dump = false;

	item = cc_list_get(managed_attributes, "index");
	// If index missing use default 0, shadow args don't have index but state has
	if (item != NULL && cc_coder_decode_uint((char *)item->data, &index) != CC_SUCCESS)
		index = 0;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_replica_identity_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	state->dump = dump;
	state->index = index;
	actor->instance_state = (void *)state;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_replica_identity_set_state(cc_actor_t *actor, cc_list_t *managed_attributes)
{
	return cc_actor_replica_identity_init(actor, managed_attributes);
}

static void cc_actor_replica_identity_did_replicate(cc_actor_t *actor, uint32_t index)
{
	((cc_actor_replica_identity_state_t*) actor->instance_state)->index = index;
}

static bool cc_actor_replica_identity_fire(struct cc_actor_t *actor)
{
	cc_token_t *token = NULL;
	cc_port_t *inport = (cc_port_t *)actor->in_ports->data; // only 1 inport
	cc_port_t *outport = (cc_port_t *)actor->out_ports->data; // only 1 outport
	char token_str[30];
	void *buffer = NULL, *buffer_next=NULL;
	uint32_t token_nbr=0;
	int token_len = 0;

	if (!cc_fifo_tokens_available(inport->fifo, 1))
	 	return false;

	if (!cc_fifo_slots_available(outport->fifo, 1))
		return false;

	token = cc_fifo_peek(inport->fifo);
	if (cc_coder_decode_uint(token->value, &token_nbr) != CC_SUCCESS) {
		cc_log_error("Failed decode input token");
		cc_fifo_cancel_commit(inport->fifo);
		return false;
	}
	token_len = snprintf(token_str, 30, "%d:%d", ((cc_actor_replica_identity_state_t*) actor->instance_state)->index, token_nbr);
	if (cc_platform_mem_alloc(&buffer, 40) != CC_SUCCESS) {
		cc_log_error("Failed alloc output token");
		cc_fifo_cancel_commit(inport->fifo);
		return false;
	}
	memset(buffer, 0, 30);
	buffer_next = cc_coder_encode_str(buffer, token_str, token_len);
	if(buffer_next == NULL) {
		cc_log_error("Failed encode output token");
		cc_fifo_cancel_commit(inport->fifo);
		cc_platform_mem_free(buffer);
		return false;
	}
	if (cc_fifo_write(outport->fifo, buffer, buffer_next - buffer) != CC_SUCCESS) {
		cc_log_error("Failed write output token");
		cc_fifo_cancel_commit(inport->fifo);
		cc_platform_mem_free(buffer);
		return false;
	}

	cc_fifo_commit_read(inport->fifo, false);

	return true;
}

static void cc_actor_replica_identity_free(cc_actor_t *actor)
{
	if (actor->instance_state)
		cc_platform_mem_free(actor->instance_state);
}

static cc_result_t cc_actor_replica_identity_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	uint32_t size = 0;
	char *buffer = NULL, *w = NULL;
	cc_actor_replica_identity_state_t *state = NULL;

	if (actor->instance_state == NULL) {
		cc_log_error("Actor does not have a state");
		return CC_FAIL;
	}

	state = (cc_actor_replica_identity_state_t *)actor->instance_state;

	size = cc_coder_sizeof_bool(state->dump);
	if (cc_platform_mem_alloc((void **)&buffer, size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	w = buffer;
	w = cc_coder_encode_bool(w, state->dump);

	if (cc_list_add_n(managed_attributes, "dump", 4, buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add 'dump' to managed list");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	size = cc_coder_sizeof_uint(state->index);
	if (cc_platform_mem_alloc((void **)&buffer, size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	w = buffer;
	w = cc_coder_encode_uint(w, state->index);

	if (cc_list_add_n(managed_attributes, "index", 5, buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add 'index' to managed list");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_replica_identity_setup(cc_actor_type_t *type)
{
	type->init = cc_actor_replica_identity_init;
	type->set_state = cc_actor_replica_identity_set_state;
	type->free_state = cc_actor_replica_identity_free;
	type->fire_actor = cc_actor_replica_identity_fire;
	type->did_replicate = cc_actor_replica_identity_did_replicate;
	type->get_managed_attributes = cc_actor_replica_identity_get_attributes;

	return CC_SUCCESS;
}
