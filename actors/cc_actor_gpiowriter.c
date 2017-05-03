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
#include "cc_actor_gpiowriter.h"
#include "../runtime/north/cc_msgpack_helper.h"
#include "../runtime/north/cc_fifo.h"

result_t actor_gpiowriter_init(actor_t **actor, list_t *attributes)
{
	uint32_t pin = 0;
	calvinsys_io_giohandler_t *gpiohandler = NULL;
	state_gpiowriter_t *state = NULL;
	char *data = NULL;

	data = (char *)list_get(attributes, "gpio_pin");
	if (data == NULL || decode_uint(data, &pin) != CC_RESULT_SUCCESS) {
		log_error("Failed to get 'gpio_pin'");
		return CC_RESULT_FAIL;
	}

	gpiohandler = (calvinsys_io_giohandler_t *)list_get((*actor)->calvinsys, "calvinsys.io.gpiohandler");
	if (gpiohandler == NULL) {
		log_error("calvinsys.io.gpiohandler is not supported");
		return CC_RESULT_FAIL;
	}

	if (gpiohandler->init_out_gpio(pin) != CC_RESULT_SUCCESS) {
		log("Failed to init gpio");
		return CC_RESULT_FAIL;
	}

	if (platform_mem_alloc((void **)&state, sizeof(state_gpiowriter_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	state->pin = pin;
	state->gpiohandler = gpiohandler;
	(*actor)->instance_state = (void *)state;

	return CC_RESULT_SUCCESS;
}

result_t actor_gpiowriter_set_state(actor_t **actor, list_t *attributes)
{
	return actor_gpiowriter_init(actor, attributes);
}

bool actor_gpiowriter_fire(struct actor_t *actor)
{
	state_gpiowriter_t *gpio_state = (state_gpiowriter_t *)actor->instance_state;
	calvinsys_io_giohandler_t *gpiohandler = gpio_state->gpiohandler;
	port_t *inport = (port_t *)actor->in_ports->data;
	token_t *in_token = NULL;
	uint32_t in_data = 0;

	if (fifo_tokens_available(&inport->fifo, 1) == 1) {
		in_token = fifo_peek(&inport->fifo);

		if (token_decode_uint(*in_token, &in_data) == CC_RESULT_SUCCESS) {
			gpiohandler->set_gpio(gpio_state->pin, in_data);
			fifo_commit_read(&inport->fifo);
			return true;
		}

		log_error("Failed to decode token");
		fifo_cancel_commit(&inport->fifo);
	}

	return false;
}

void actor_gpiowriter_free(actor_t *actor)
{
	state_gpiowriter_t *gpio_state = (state_gpiowriter_t *)actor->instance_state;
	calvinsys_io_giohandler_t *gpiohandler = gpio_state->gpiohandler;

	gpiohandler->uninit_gpio(gpiohandler, gpio_state->pin, CALVIN_GPIO_OUT);
	platform_mem_free((void *)gpio_state);
}

result_t actor_gpiowriter_get_managed_attributes(actor_t *actor, list_t **attributes)
{
	// TODO: Implement
	return CC_RESULT_SUCCESS;
}
