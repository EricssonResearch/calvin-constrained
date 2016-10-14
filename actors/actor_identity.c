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
#include "actor_identity.h"
#include "../port.h"
#include "../token.h"
#include "../msgpack_helper.h"
#include "../platform.h"

result_t actor_identity_init(actor_t **actor, char *obj_actor_state, actor_state_t **state)
{
	result_t result = SUCCESS;
	state_identity_t *identity_state = NULL;
	char *obj_shadow_args = NULL;

	*state = (actor_state_t *)malloc(sizeof(actor_state_t));
	if (*state == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	identity_state = (state_identity_t *)malloc(sizeof(state_identity_t));
	if (identity_state == NULL) {
		log_error("Failed to allocate memory");
		free(*state);
		return FAIL;
	}

	if (has_key(&obj_actor_state, "_shadow_args")) {
		result = get_value_from_map(&obj_actor_state, "_shadow_args", &obj_shadow_args);
		if (result == SUCCESS) {
			result = decode_bool_from_map(&obj_shadow_args, "dump", &identity_state->dump);
			if (result == SUCCESS)
				result = add_managed_attribute(actor, "dump");
		}
	} else
		result = decode_bool_from_map(&obj_actor_state, "dump", &identity_state->dump);

	if (result == SUCCESS) {
		(*state)->state = (void *)identity_state;
	} else {
		free(*state);
		free(identity_state);
	}

	return result;
}

result_t actor_identity_fire(struct actor_t *actor)
{
	port_t *port = actor->inports;
	token_t *token = NULL;
	result_t result = SUCCESS;

	if (port->fifo != NULL) {
		while (result == SUCCESS && fifo_can_read(port->fifo)) {
			token = fifo_read(port->fifo);
			result = fifo_write(actor->outports->fifo, token);
			if (result != SUCCESS) {
				fifo_commit_read(port->fifo, false, false);
				result = FAIL;
			} else {
				fifo_commit_read(port->fifo, true, false);
			}
		}
	}

	return result;
}

char *actor_identity_serialize(actor_state_t *state, char **buffer)
{
	*buffer = encode_bool(buffer, "dump", false);

	return *buffer;
}

void actor_identity_free(actor_t *actor)
{
	state_identity_t *state = NULL;

	if (actor->state != NULL) {
		if (actor->state->state != NULL)
			free(state);
		free(actor->state);
	}
}
