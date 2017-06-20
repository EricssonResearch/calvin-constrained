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
#include "../msgpuck/msgpuck.h"

result_t actor_button_init(actor_t **actor, list_t *attributes)
{
	calvinsys_obj_t *obj = NULL;

	obj = calvinsys_open((*actor)->calvinsys, "calvinsys.io.button", NULL, 0);
	if (obj == NULL) {
		log_error("Failed to open 'calvinsys.io.button'");
		return CC_RESULT_FAIL;
	}

	(*actor)->instance_state = (void *)obj;

	return CC_RESULT_SUCCESS;
}

result_t actor_button_set_state(actor_t **actor, list_t *attributes)
{
	return actor_button_init(actor, attributes);
}

bool actor_button_fire(struct actor_t *actor)
{
	port_t *outport = (port_t *)actor->out_ports->data;
	calvinsys_obj_t *obj = (calvinsys_obj_t *)actor->instance_state;
	char *data = NULL;
	size_t size = 0;

	if (obj->can_read(obj) && fifo_slots_available(outport->fifo, 1)) {
		if (obj->read(obj, &data, &size) == CC_RESULT_SUCCESS) {
			if (fifo_write(outport->fifo, data, size) == CC_RESULT_SUCCESS)
				return true;
			platform_mem_free((void *)data);
		} else
			log_error("Failed to read button state");
	}

	return false;
}

void actor_button_free(actor_t *actor)
{
	calvinsys_close((calvinsys_obj_t *)actor->instance_state);
}
