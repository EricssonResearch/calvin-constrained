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
#include "actor_gpioreader.h"
#include "../msgpack_helper.h"
#include "../fifo.h"
#include "../token.h"

result_t actor_gpioreader_init(actor_t **actor, list_t *attributes)
{
	state_gpioreader_t *state = NULL;
	char *tmp_string;
	uint32_t tmp_string_len = 0;
	list_t *tmp = NULL;

	if (platform_mem_alloc((void **)&state, sizeof(state_gpioreader_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	tmp = list_get(attributes, "gpio_pin");
	if (tmp == NULL || decode_uint((char *)tmp->data, &state->pin) != SUCCESS) {
		log_error("Failed to get 'gpio_pin'");
		return FAIL;
	}

	tmp = list_get(attributes, "pull");
	if (tmp == NULL || decode_str((char *)tmp->data, (char **)&tmp_string, &tmp_string_len) != SUCCESS) {
		log_error("Failed to get 'pull'");
		return FAIL;
	}
	state->pull = tmp_string[0];

	tmp = list_get(attributes, "edge");
	if (tmp == NULL || decode_str((char *)tmp->data, &tmp_string, &tmp_string_len) != SUCCESS) {
		log_error("Failed to get 'edge'");
		return FAIL;
	}
	state->edge = tmp_string[0];

	state->gpio = platform_create_in_gpio(state->pin, state->pull, state->edge);
	if (state->gpio == NULL) {
		log_error("Failed create gpio");
		return FAIL;
	}

	(*actor)->instance_state = (void *)state;

	return SUCCESS;
}

result_t actor_gpioreader_set_state(actor_t **actor, list_t *attributes)
{
	return actor_gpioreader_init(actor, attributes);
}

result_t actor_gpioreader_fire(struct actor_t *actor)
{
	token_t out_token;
	port_t *outport = NULL;
	state_gpioreader_t *state = (state_gpioreader_t *)actor->instance_state;

	if (!state->gpio->has_triggered)
		return FAIL;

	outport = port_get_from_name(actor, "state", PORT_DIRECTION_OUT);
	if (outport == NULL) {
		log_error("No port with name 'state'");
		return FAIL;
	}

	if (fifo_slots_available(&outport->fifo, 1) == 1) {
		token_set_uint(&out_token, state->gpio->value);

		if (fifo_write(&outport->fifo, out_token.value, out_token.size) != SUCCESS) {
			log_error("Failed to write token");
			return FAIL;
		}
	}

	state->gpio->has_triggered = false;

	return SUCCESS;
}

void actor_gpioreader_free(actor_t *actor)
{
	state_gpioreader_t *state = (state_gpioreader_t *)actor->instance_state;

	if (state != NULL) {
		if (state->gpio != NULL)
			platform_uninit_gpio(state->gpio);
		platform_mem_free((void *)state);
	}
}

result_t actor_gpioreader_get_managed_attributes(actor_t *actor, list_t **attributes)
{
	// TODO: Implement
	return SUCCESS;
}
