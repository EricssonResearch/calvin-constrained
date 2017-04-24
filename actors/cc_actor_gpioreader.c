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
#include "cc_actor_gpioreader.h"
#include "../runtime/north/cc_msgpack_helper.h"
#include "../runtime/north/cc_fifo.h"
#include "../runtime/north/cc_token.h"

result_t actor_gpioreader_init(actor_t **actor, list_t *attributes)
{
	state_gpioreader_t *state = NULL;
	char *data = NULL, *pull = NULL, *edge = NULL;
	uint32_t pin = 0, len = 0;
	calvinsys_io_giohandler_t *gpiohandler = NULL;

	gpiohandler = (calvinsys_io_giohandler_t *)list_get((*actor)->calvinsys, "calvinsys.io.gpiohandler");
	if (gpiohandler == NULL) {
		log_error("calvinsys.io.gpiohandler is not supported");
		return FAIL;
	}

	data = (char *)list_get(attributes, "gpio_pin");
	if (data == NULL || decode_uint((char *)data, &pin) != SUCCESS) {
		log_error("Failed to get 'gpio_pin'");
		return FAIL;
	}

	data = (char *)list_get(attributes, "pull");
	if (data == NULL || decode_str(data, (char **)&pull, &len) != SUCCESS) {
		log_error("Failed to get 'pull'");
		return FAIL;
	}

	data = (char *)list_get(attributes, "edge");
	if (data == NULL || decode_str(data, (char **)&edge, &len) != SUCCESS) {
		log_error("Failed to get 'edge'");
		return FAIL;
	}

	if (platform_mem_alloc((void **)&state, sizeof(state_gpioreader_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	state->gpio = gpiohandler->init_in_gpio(gpiohandler, pin, pull[0], edge[0]);
	if (state->gpio == NULL) {
		log_error("Failed create gpio");
		return FAIL;
	}
	state->gpiohandler = gpiohandler;

	(*actor)->instance_state = (void *)state;

	return SUCCESS;
}

result_t actor_gpioreader_set_state(actor_t **actor, list_t *attributes)
{
	return actor_gpioreader_init(actor, attributes);
}

bool actor_gpioreader_fire(struct actor_t *actor)
{
	token_t out_token;
	port_t *outport = (port_t *)actor->out_ports->data;
	state_gpioreader_t *gpio_state = (state_gpioreader_t *)actor->instance_state;

	if (gpio_state->gpio->has_triggered) {
		if (fifo_slots_available(&outport->fifo, 1) == 1) {
			token_set_uint(&out_token, gpio_state->gpio->value);
			if (fifo_write(&outport->fifo, out_token.value, out_token.size) == SUCCESS) {
				gpio_state->gpio->has_triggered = false;
				return true;
			}
		}
	}

	return false;
}

void actor_gpioreader_free(actor_t *actor)
{
	state_gpioreader_t *gpio_state = (state_gpioreader_t *)actor->instance_state;
	calvinsys_io_giohandler_t *gpiohandler = gpio_state->gpiohandler;

	gpiohandler->uninit_gpio(gpiohandler, gpio_state->gpio->pin, GPIO_IN);
	platform_mem_free((void *)gpio_state);
}

result_t actor_gpioreader_get_managed_attributes(actor_t *actor, list_t **attributes)
{
	return SUCCESS;
}
