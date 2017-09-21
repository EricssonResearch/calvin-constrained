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

static cc_result_t cc_actor_identity_init(cc_actor_t**actor, cc_list_t *attributes)
{
	state_identity_t *state = NULL;
	bool dump = false;
	char *data = NULL;

	data = (char *)cc_list_get(attributes, "dump");
	if (data == NULL) {
		cc_log_error("Failed to get attribute 'dump'");
		return CC_FAIL;
	}

	if (cc_coder_decode_bool(data, &dump) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_platform_mem_alloc((void **)&state, sizeof(state_identity_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	state->dump = dump;
	(*actor)->instance_state = (void *)state;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_identity_set_state(cc_actor_t**actor, cc_list_t *attributes)
{
	return cc_actor_identity_init(actor, attributes);
}

static bool cc_actor_identity_fire(struct cc_actor_t*actor)
{
	cc_token_t *token = NULL;
	cc_port_t *inport = (cc_port_t *)actor->in_ports->data, *outport = (cc_port_t *)actor->out_ports->data;

	if (cc_fifo_tokens_available(inport->fifo, 1) && cc_fifo_slots_available(outport->fifo, 1)) {
		token = cc_fifo_peek(inport->fifo);
		if (cc_fifo_write(outport->fifo, token->value, token->size) == CC_SUCCESS) {
			cc_fifo_commit_read(inport->fifo, false);
			return true;
		}
		cc_fifo_cancel_commit(inport->fifo);
	}
	return false;
}

static void cc_actor_identity_free(cc_actor_t*actor)
{
	state_identity_t *state = (state_identity_t *)actor->instance_state;

	if (state != NULL)
		cc_platform_mem_free((void *)state);
}

static cc_result_t cc_actor_identity_get_managed_attributes(cc_actor_t*actor, cc_list_t **attributes)
{
	uint32_t size = 0;
	char *value = NULL, *name = NULL;
	state_identity_t *state = (state_identity_t *)actor->instance_state;

	if (cc_platform_mem_alloc((void **)&name, 5) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	strncpy(name, "dump", 5);

	size = cc_coder_sizeof_bool(state->dump);
	if (cc_platform_mem_alloc((void **)&value, size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	cc_coder_encode_bool(value, state->dump);

	if (cc_list_add(attributes, name, value, size) != CC_SUCCESS) {
		cc_log_error("Failed to add '%s' to managed list", name);
		cc_platform_mem_free(name);
		cc_platform_mem_free(value);
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

	type->init = cc_actor_identity_init;
	type->set_state = cc_actor_identity_set_state;
	type->free_state = cc_actor_identity_free;
	type->fire_actor = cc_actor_identity_fire;
	type->get_managed_attributes = cc_actor_identity_get_managed_attributes;
	type->will_migrate = NULL;
	type->will_end = NULL;
	type->did_migrate = NULL;
	return cc_list_add_n(actor_types, "std.Identity", 12, type, sizeof(cc_actor_type_t *));
}
