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
#include "actor_temperature.h"
#include "../fifo.h"
#include "../token.h"
#include "../port.h"
#include "../common.h"

result_t actor_temperature_init(actor_t **actor, list_t *attributes)
{
	calvinsys_sensors_environmental_t *environmental = NULL;
	state_temperature_t *state = NULL;

	environmental = (calvinsys_sensors_environmental_t *)list_get((*actor)->calvinsys, "calvinsys.sensors.environmental");
	if (environmental == NULL) {
		log_error("calvinsys.sensors.environmental is not supported");
		return FAIL;
	}

	if (platform_mem_alloc((void **)&state, sizeof(state_temperature_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	state->environmental = environmental;
	(*actor)->instance_state = (void *)state;

	return SUCCESS;
}

result_t actor_temperature_set_state(actor_t **actor, list_t *attributes)
{
	return actor_temperature_init(actor, attributes);
}

bool actor_temperature_fire(struct actor_t *actor)
{
	token_t out_token;
	double temperature;
	port_t *inport = (port_t *)actor->in_ports->data, *outport = (port_t *)actor->out_ports->data;
	calvinsys_sensors_environmental_t *environmental = ((state_temperature_t *)actor->instance_state)->environmental;

	if (fifo_tokens_available(&inport->fifo, 1) == 1 && fifo_slots_available(&outport->fifo, 1) == 1) {
		if (environmental->get_temperature(&temperature) == SUCCESS) {
			fifo_peek(&inport->fifo);
			token_set_double(&out_token, temperature);
			if (fifo_write(&outport->fifo, out_token.value, out_token.size) == SUCCESS) {
				fifo_commit_read(&inport->fifo);
				return true;
			}
			fifo_cancel_commit(&inport->fifo);
		} else
			log_error("Failed to get temperature");
	}
	return false;
}

void actor_temperature_free(actor_t *actor)
{
	state_temperature_t *state = (state_temperature_t *)actor->instance_state;

	if (state != NULL)
		platform_mem_free((void *)state);
}
