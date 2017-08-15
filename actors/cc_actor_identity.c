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
//#include <string.h>
#include "cc_actor_identity.h"
#include "../runtime/north/cc_actor_store.h"
#include "../runtime/north/cc_port.h"
#include "../runtime/north/cc_token.h"
#include "../runtime/north/cc_msgpack_helper.h"
#include "../runtime/south/platform/cc_platform.h"
#include "../msgpuck/msgpuck.h"

typedef struct state_identity_t {
	bool dump;
} state_identity_t;

static result_t actor_identity_init(actor_t **actor, list_t *attributes)
{
	state_identity_t *state = NULL;
	bool dump = false;
	char *data = NULL;

	data = (char *)list_get(attributes, "dump");
	if (data == NULL) {
		cc_log_error("Failed to get attribute 'dump'");
		return CC_RESULT_FAIL;
	}

	if (decode_bool(data, &dump) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (platform_mem_alloc((void **)&state, sizeof(state_identity_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	state->dump = dump;
	(*actor)->instance_state = (void *)state;

	return CC_RESULT_SUCCESS;
}

static result_t actor_identity_set_state(actor_t **actor, list_t *attributes)
{
	return actor_identity_init(actor, attributes);
}

static bool actor_identity_fire(struct actor_t *actor)
{
	token_t *token = NULL;
	port_t *inport = (port_t *)actor->in_ports->data, *outport = (port_t *)actor->out_ports->data;

	if (fifo_tokens_available(inport->fifo, 1) && fifo_slots_available(outport->fifo, 1)) {
		token = fifo_peek(inport->fifo);
		if (fifo_write(outport->fifo, token->value, token->size) == CC_RESULT_SUCCESS) {
			fifo_commit_read(inport->fifo, false);
			return true;
		}
		fifo_cancel_commit(inport->fifo);
	}
	return false;
}

static void actor_identity_free(actor_t *actor)
{
	state_identity_t *state = (state_identity_t *)actor->instance_state;

	if (state != NULL)
		platform_mem_free((void *)state);
}

static result_t actor_identity_get_managed_attributes(actor_t *actor, list_t **attributes)
{
	uint32_t size = 0;
	char *value = NULL, *name = NULL;
	state_identity_t *state = (state_identity_t *)actor->instance_state;

	if (platform_mem_alloc((void **)&name, 5) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}
	strncpy(name, "dump", 5);

	size = mp_sizeof_bool(state->dump);
	if (platform_mem_alloc((void **)&value, size) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	mp_encode_bool(value, state->dump);

	if (list_add(attributes, name, value, size) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to add '%s' to managed list", name);
		platform_mem_free(name);
		platform_mem_free(value);
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

result_t actor_identity_register(list_t **actor_types)
{
	actor_type_t *type = NULL;

	if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	type->init = actor_identity_init;
	type->set_state = actor_identity_set_state;
	type->free_state = actor_identity_free;
	type->fire_actor = actor_identity_fire;
	type->get_managed_attributes = actor_identity_get_managed_attributes;
	type->will_migrate = NULL;
	type->will_end = NULL;
	type->did_migrate = NULL;
	return list_add_n(actor_types, "std.Identity", 12, type, sizeof(actor_type_t *));
}
