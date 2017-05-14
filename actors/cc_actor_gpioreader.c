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
#include "../calvinsys/cc_calvinsys.h"
#include "../msgpuck/msgpuck.h"

result_t actor_gpioreader_init(actor_t **actor, list_t *attributes)
{
	calvinsys_obj_t *obj = NULL;
	state_gpioreader_t *state = NULL;
	char buffer[100], *tmp = buffer;
	char *pin, *pull = NULL, *edge = NULL;

	pin = (char *)list_get(attributes, "gpio_pin");
	if (pin == NULL) {
		log_error("Failed to get 'pin'");
		return CC_RESULT_FAIL;
	}

	pull = (char *)list_get(attributes, "pull");
	if (pull == NULL) {
		log_error("Failed to get 'pull'");
		return CC_RESULT_FAIL;
	}

	edge = (char *)list_get(attributes, "edge");
	if (edge == NULL) {
		log_error("Failed to get 'edge'");
		return CC_RESULT_FAIL;
	}

	if (platform_mem_alloc((void **)&state, sizeof(state_gpioreader_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	tmp = mp_encode_map(tmp, 4);
	{
		tmp = encode_str(&tmp, "direction", "i", 1);
		tmp = encode_value(&tmp, "pin", pin, get_size_of_value(pin));
		tmp = encode_value(&tmp, "pull", pull, get_size_of_value(pull));
		tmp = encode_value(&tmp, "edge", edge, get_size_of_value(edge));
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

result_t actor_gpioreader_set_state(actor_t **actor, list_t *attributes)
{
	return actor_gpioreader_init(actor, attributes);
}

bool actor_gpioreader_fire(struct actor_t *actor)
{
	port_t *outport = (port_t *)actor->out_ports->data;
	calvinsys_obj_t *obj = ((state_gpioreader_t *)actor->instance_state)->obj;
	char *data = NULL;
	size_t size = 0;

	if (obj->can_read(obj) && fifo_slots_available(&outport->fifo, 1) == 1) {
		if (obj->read(obj, &data, &size) == CC_RESULT_SUCCESS) {
			if (fifo_write(&outport->fifo, data, size) == CC_RESULT_SUCCESS)
				return true;
			platform_mem_free((void *)data);
		}
	}

	return false;
}

void actor_gpioreader_free(actor_t *actor)
{
	state_gpioreader_t *state = (state_gpioreader_t *)actor->instance_state;

	if (state != NULL) {
		if (state->obj != NULL)
			calvinsys_close(state->obj);
		platform_mem_free((void *)state);
	}
}
