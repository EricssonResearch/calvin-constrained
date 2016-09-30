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
#include "actor_counttimer.h"
#include "../msgpack_helper.h"
#include "../fifo.h"
#include "../msgpuck/msgpuck.h"

result_t actor_count_timer_init(char *obj_actor_state, actor_state_t **state)
{
	result_t result = SUCCESS;
	state_count_timer_t *count_state = NULL;

	*state = (actor_state_t *)malloc(sizeof(actor_state_t));
	if (*state == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	count_state = (state_count_timer_t *)malloc(sizeof(state_count_timer_t));
	if (count_state == NULL) {
		log_error("Failed to allocate memory");
		free(*state);
		return FAIL;
	}

	result = decode_uint_from_map(&obj_actor_state, "steps", &count_state->steps);

	if (result == SUCCESS)
		result = decode_uint_from_map(&obj_actor_state, "start", &count_state->start);

	if (result == SUCCESS)
		result = decode_double_from_map(&obj_actor_state, "sleep", &count_state->sleep);

	if (result == SUCCESS)
		result = decode_uint_from_map(&obj_actor_state, "count", &count_state->count);

	if (result == SUCCESS) {
		count_state->timer = create_recurring_timer(count_state->sleep);
		if (count_state->timer == NULL)
			result = FAIL;
	}

	if (result == SUCCESS) {
		(*state)->nbr_attributes = 4;
		(*state)->state = (void *)count_state;
	} else {
		free(*state);
		free(count_state);
	}

	return result;
}

result_t actor_count_timer(struct actor_t *actor)
{
	token_t *token = NULL;
	state_count_timer_t *state = (state_count_timer_t *)actor->state->state;

	if (check_timer(state->timer)) {
		if (fifo_can_write(actor->outports->fifo)) {
			if (create_uint_token(state->count + 1, &token) == SUCCESS) {
				if (fifo_write(actor->outports->fifo, token) == SUCCESS) {
					state->count += 1;
					return SUCCESS;
				}
				log_error("Failed to write to fifo");
				free_token(token);
			} else {
				log_error("Failed to create token");
			}
		}
	}

	return FAIL;
}

void free_count_timer_state(actor_t *actor)
{
	state_count_timer_t *state = NULL;

	if (actor->state != NULL) {
		if (actor->state->state != NULL) {
			state = (state_count_timer_t *)actor->state->state;
			if (state->timer != NULL)
				stop_timer(state->timer);
			free(state);
		}
		free(actor->state);
	}
}

char *serialize_count_timer(actor_state_t *state, char **buffer)
{
	state_count_timer_t *count_state = NULL;

	if (state != NULL && state->state != NULL) {
		count_state = (state_count_timer_t *)state->state;
		*buffer = encode_double(buffer, "sleep", count_state->sleep);
		*buffer = encode_uint(buffer, "count", count_state->count);
		*buffer = encode_uint(buffer, "steps", count_state->steps);
		*buffer = encode_uint(buffer, "start", count_state->start);
	}

	return *buffer;
}
