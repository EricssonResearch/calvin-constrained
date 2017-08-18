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
#include "cc_actor_gyroscope.h"
#include "../runtime/north/cc_actor_store.h"

static result_t actor_gyroscope_init(actor_t **actor, list_t *attributes)
{
	calvinsys_obj_t *obj = NULL;

	obj = calvinsys_open((*actor)->calvinsys, "io.gyroscope", NULL, 0);
	if (obj == NULL) {
		cc_log_error("Failed to open 'io.gyroscope'");
		return CC_RESULT_FAIL;
	}

	(*actor)->instance_state = (void *)obj;

	return CC_RESULT_SUCCESS;
}

static result_t actor_gyroscope_set_state(actor_t **actor, list_t *attributes)
{
	return actor_gyroscope_init(actor, attributes);
}

static bool actor_gyroscope_fire(actor_t *actor)
{
	port_t *inport = (port_t *)actor->in_ports->data, *outport = (port_t *)actor->out_ports->data;
	calvinsys_obj_t *obj = (calvinsys_obj_t *)actor->instance_state;
	char *data = NULL;
	size_t size = 0;

	if (obj->can_read(obj) && fifo_tokens_available(inport->fifo, 1) && fifo_slots_available(outport->fifo, 1)) {
		if (obj->read(obj, &data, &size) == CC_RESULT_SUCCESS) {
			fifo_peek(inport->fifo);
			if (fifo_write(outport->fifo, data, size) == CC_RESULT_SUCCESS) {
				fifo_commit_read(inport->fifo, true);
				return true;
			}
			cc_log_error("Could not write to ouport");
			fifo_cancel_commit(inport->fifo);
			platform_mem_free((void *)data);
		} else
			cc_log_error("Failed to read value");
	} else {
		cc_log_error("could not read from gyroscope");
	}
	return false;
}

static void actor_gyroscope_free(actor_t *actor)
{
	cc_log("gyroscope close");
	calvinsys_close((calvinsys_obj_t *)actor->instance_state);
}

result_t actor_gyroscope_register(list_t **actor_types)
{
	actor_type_t *type = NULL;

	if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	type->init = actor_gyroscope_init;
	type->set_state = actor_gyroscope_set_state;
	type->free_state = actor_gyroscope_free;
	type->fire_actor = actor_gyroscope_fire;
	type->get_managed_attributes = NULL;
	type->will_migrate = NULL;
	type->will_end = NULL;
	type->did_migrate = NULL;

	return list_add_n(actor_types, "sensor.TriggeredGyroscope", 25, type, sizeof(actor_type_t *));

}
