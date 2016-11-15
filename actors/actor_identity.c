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
#include "actor_identity.h"
#include "../port.h"
#include "../token.h"
#include "../msgpack_helper.h"
#include "../platform.h"

result_t actor_identity_init(actor_t **actor, char *obj_actor_state)
{
    state_identity_t *state = NULL;

    if (platform_mem_alloc((void **)&state, sizeof(state_identity_t)) != SUCCESS) {
        log_error("Failed to allocate memory");
        return FAIL;
    }

    state->managed_attributes = NULL;
    state->dump = false;

    if (actor_get_managed(obj_actor_state, &state->managed_attributes) != SUCCESS) {
        platform_mem_free((void *)state);
        return FAIL;
    }

    (*actor)->instance_state = (void *)state;

    return SUCCESS;
}

result_t actor_identity_set_state(actor_t **actor, char *obj_actor_state)
{
    result_t result = SUCCESS;
    state_identity_t *state = NULL;
    list_t *list = NULL; 

    if (platform_mem_alloc((void **)&state, sizeof(state_identity_t)) != SUCCESS) {
        log_error("Failed to allocate memory");
        return FAIL;
    }

    state->managed_attributes = NULL;
    state->dump = false;

    if (actor_get_managed(obj_actor_state, &state->managed_attributes) != SUCCESS) {
        platform_mem_free((void *)state);
        return FAIL;
    }
    
    list = state->managed_attributes;
    while (list != NULL && result == SUCCESS) {
        if (strncmp(list->id, "dump", strlen("dump")) == 0) {
            result = decode_bool((char *)list->data, &state->dump);
        }
        list = list->next;
    }
    
    if (result != SUCCESS) {
        log_error("Failed to parse attributes");
        actor_free_managed(state->managed_attributes);
        platform_mem_free((void *)state);
        return FAIL;
    }

    (*actor)->instance_state = (void *)state;

    return SUCCESS; 
}

result_t actor_identity_fire(struct actor_t *actor)
{
	token_t *in_token = NULL, out_token;
	uint32_t in_data = 0;
	port_t *inport = NULL, *outport = NULL;
	bool did_fire = false;

	memset(&out_token, 0, sizeof(token_t));

	inport = port_get_from_name(actor, "token", PORT_DIRECTION_IN);
	if (inport == NULL) {
		log_error("No port with name 'token'");
		return FAIL;
	}

	outport = port_get_from_name(actor, "token", PORT_DIRECTION_OUT);
	if (outport == NULL) {
		log_error("No port with name 'token'");
		return FAIL;
	}

	if (fifo_tokens_available(&inport->fifo, 1) == 1 && fifo_slots_available(&outport->fifo, 1) == 1) {
		in_token = fifo_peek(&inport->fifo);
		if (token_decode_uint(*in_token, &in_data) != SUCCESS) {
			log_error("Failed to decode token");
			fifo_cancel_commit(&inport->fifo);
			return FAIL;
		}

		token_set_uint(&out_token, in_data);

		if (fifo_write(&outport->fifo, out_token.value, out_token.size) != SUCCESS) {
			log_error("Failed to write token");
			fifo_cancel_commit(&inport->fifo);
			return FAIL;
		}

		fifo_commit_read(&inport->fifo);
		did_fire = true;
	}

	if (did_fire)
		return SUCCESS;

	return FAIL;
}

void actor_identity_free(actor_t *actor)
{
    state_identity_t *state = (state_identity_t *)actor->instance_state;

    if (state != NULL) {
        actor_free_managed(state->managed_attributes);
        platform_mem_free((void *)state);
    }
}

char *actor_identity_serialize(actor_t *actor, char **buffer)
{
    state_identity_t *state = (state_identity_t *)actor->instance_state;

    *buffer = actor_serialize_managed_list(state->managed_attributes, buffer);
    if (list_get(state->managed_attributes, "dump") == NULL)
        *buffer = encode_bool(buffer, "dump", state->dump);
 
    return *buffer;
}

list_t *actor_identity_get_managed_attributes(actor_t *actor)
{
    state_identity_t *state = (state_identity_t *)actor->instance_state;
    
    return state->managed_attributes;
}
