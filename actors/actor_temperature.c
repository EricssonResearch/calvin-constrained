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
#include "actor_temperature.h"
#include "../fifo.h"
#include "../token.h"
#include "../platform.h"

result_t actor_temperature_fire(struct actor_t *actor)
{
	token_t *out_token = NULL;
	double temperature;

	if (!fifo_can_read(actor->inports->fifo) || !fifo_can_write(actor->outports->fifo))
		return SUCCESS;

	if (get_temperature(&temperature) != SUCCESS) {
		log_error("Failed to get temperature");
		return FAIL;
	}

	if (create_double_token(temperature, &out_token) != SUCCESS) {
		log_error("Failed to create token");
		return FAIL;
	}

	fifo_read(actor->inports->fifo);

	if (fifo_write(actor->outports->fifo, out_token) == SUCCESS) {
		fifo_commit_read(actor->inports->fifo, true, true);
		return SUCCESS;
	}

	log_error("Failed to write token");
	fifo_commit_read(actor->inports->fifo, false, false);
	return FAIL;
}
