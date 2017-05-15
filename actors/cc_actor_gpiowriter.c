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
#include "../msgpuck/msgpuck.h"

result_t actor_gpiowriter_init(actor_t **actor, list_t *attributes)
{
	calvinsys_obj_t *obj = NULL;
	state_gpiowriter_t *state = NULL;
	char buffer[100], *tmp = buffer;
	char *pin;

	pin = (char *)list_get(attributes, "gpio_pin");
	if (pin == NULL) {
		log_error("Failed to get 'pin'");
		return CC_RESULT_FAIL;
	}

	if (platform_mem_alloc((void **)&state, sizeof(state_gpiowriter_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	tmp = mp_encode_map(tmp, 2);
	{
		tmp = encode_str(&tmp, "direction", "o", 1);
		tmp = encode_value(&tmp, "pin", pin, get_size_of_value(pin));
	}

	obj = calvinsys_open((*actor)->calvinsys, "calvinsys.io.gpiohandler", buffer, tmp - buffer);
	if (obj == NULL) {
		log_error("Failed to open 'calvinsys.io.gpiohandler'");
		return CC_RESULT_FAIL;
	}

	state->obj = obj;
	(*actor)->instance_state = (void *)state;

	return CC_RESULT_SUCCESS;
}

result_t actor_gpiowriter_set_state(actor_t **actor, list_t *attributes)
{
	return actor_gpiowriter_init(actor, attributes);
}

bool actor_gpiowriter_fire(struct actor_t *actor)
{
	port_t *inport = (port_t *)actor->in_ports->data;
	calvinsys_obj_t *obj = ((state_gpiowriter_t *)actor->instance_state)->obj;
	token_t *token = NULL;

	if (fifo_tokens_available(&inport->fifo, 1)) {
		token = fifo_peek(&inport->fifo);
		if (obj->write(obj, NULL, 0, token->value, token->size) == CC_RESULT_SUCCESS) {
			fifo_commit_read(&inport->fifo);
			return true;
		}
		fifo_cancel_commit(&inport->fifo);
	}

	return false;
}

void actor_gpiowriter_free(actor_t *actor)
{
	state_gpiowriter_t *state = (state_gpiowriter_t *)actor->instance_state;

	if (state != NULL) {
		if (state->obj != NULL)
			calvinsys_close(state->obj);
		platform_mem_free((void *)state);
	}
}
