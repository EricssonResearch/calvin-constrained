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
#include "../platform.h"
#include "../port.h"

bool actor_temperature_fire(struct actor_t *actor)
{
	token_t out_token;
	double temperature;
	port_t *inport = (port_t *)actor->in_ports->data, *outport = (port_t *)actor->out_ports->data;

	if (fifo_tokens_available(&inport->fifo, 1) == 1 && fifo_slots_available(&outport->fifo, 1) == 1) {
		if (platform_get_temperature(&temperature) == SUCCESS) {
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
