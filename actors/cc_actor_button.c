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
#include "cc_actor_button.h"
#include "../runtime/north/cc_port.h"
#include "../runtime/north/cc_token.h"
#include "../runtime/north/cc_msgpack_helper.h"
#include "../runtime/south/platform/cc_platform.h"
#include "../msgpuck/msgpuck.h"

result_t actor_button_init(actor_t **actor, list_t *attributes)
{
	calvinsys_t* button = (calvinsys_t*) list_get((*actor)->calvinsys, "calvinsys.io.button");
	if (button == NULL) {
		log_error("calvinsys.io.button not supported");
		return FAIL;
	}
	state_button_t* state;
	if (platform_mem_alloc((void**)&state, sizeof(state_button_t)) != SUCCESS) {
		log_error("Could not allocate memory for button state");
		return FAIL;
	}
	log("Button init");
	button->init(button);
	state->button = button;
	(*actor)->instance_state = (void*) state;
	return SUCCESS;
}

bool actor_button_fire(struct actor_t *actor)
{
	log("button fire");
	if (actor->instance_state != NULL) {
		calvinsys_t *button = ((state_button_t *) actor->instance_state)->button;
		if (button != NULL && button->new_data) {
			if (strncmp(button->command, "trigger", 7) == 0) {
				token_t out_token;
				port_t *outport = (port_t *) actor->out_ports->data;
				if (fifo_slots_available(&outport->fifo, 1) == 1) {
					token_set_uint(&out_token, 1);
					if (fifo_write(&outport->fifo, out_token.value, out_token.size) != SUCCESS) {
						log_error("sent button pressed token failed");
					} else {
						button->new_data = 0;
						return true;
					}
				}
			}
		}
	}
	return false;
}
result_t actor_button_set_state(actor_t **actor, list_t *attributes)
{
	return actor_button_init(actor, attributes);
}

void actor_button_will_end(actor_t* actor)
{
	log("button will end");
	if(actor->instance_state != NULL) {
		calvinsys_t *button = ((state_button_t *) actor->instance_state)->button;
		button->release(button);
	}
}

void actor_button_free(actor_t *actor)
{
	log("button free");
	if(actor->instance_state != NULL) {
		calvinsys_t *button = ((state_button_t *) actor->instance_state)->button;
		release_calvinsys(&button);
		platform_mem_free(actor->instance_state);
	}
}
