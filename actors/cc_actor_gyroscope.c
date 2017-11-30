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
#include <string.h>
#include "cc_actor_gyroscope.h"
#include "runtime/north/cc_actor_store.h"

static cc_result_t cc_actor_gyroscope_init(cc_actor_t **actor, cc_list_t *attributes)
{
	char *obj_ref = NULL;

	obj_ref = cc_calvinsys_open(*actor, "io.gyroscope", NULL);
	if (obj_ref == NULL) {
		cc_log_error("Failed to open 'io.gyroscope'");
		return CC_FAIL;
	}

	(*actor)->instance_state = (void *)obj_ref;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_gyroscope_set_state(cc_actor_t **actor, cc_list_t *attributes)
{
	return cc_actor_gyroscope_init(actor, attributes);
}

static bool cc_actor_gyroscope_fire(cc_actor_t *actor)
{
	cc_port_t *inport = (cc_port_t *)actor->in_ports->data, *outport = (cc_port_t *)actor->out_ports->data;
	char *obj_ref = (char *)actor->instance_state;
	char *data = NULL;
	size_t size = 0;

	if (cc_calvinsys_can_read(actor->calvinsys, obj_ref) && cc_fifo_tokens_available(inport->fifo, 1) && cc_fifo_slots_available(outport->fifo, 1)) {
		if (cc_calvinsys_read(actor->calvinsys, obj_ref, &data, &size) == CC_SUCCESS) {
			cc_fifo_peek(inport->fifo);
			if (cc_fifo_write(outport->fifo, data, size) == CC_SUCCESS) {
				cc_fifo_commit_read(inport->fifo, true);
				return true;
			}
			cc_log_error("Could not write to ouport");
			cc_fifo_cancel_commit(inport->fifo);
			cc_platform_mem_free((void *)data);
		} else
			cc_log_error("Failed to read value");
	} else {
		cc_log_error("could not read from gyroscope");
	}
	return false;
}

cc_result_t cc_actor_gyroscope_get_requires(cc_actor_t *actor, cc_list_t **requires)
{
	if (cc_list_add_n(requires, "io.gyroscope", 12, NULL, 0) == NULL) {
		cc_log_error("Failed to add requires");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_gyroscope_setup(cc_actor_type_t *type)
{
	type->init = cc_actor_gyroscope_init;
	type->set_state = cc_actor_gyroscope_set_state;
	type->fire_actor = cc_actor_gyroscope_fire;
	type->get_requires = cc_actor_gyroscope_get_requires;

	return CC_SUCCESS;
}
