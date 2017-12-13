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
#include "runtime/north/cc_actor.h"
#include "runtime/north/cc_actor_store.h"

static cc_result_t cc_actor_camera_init(cc_actor_t **actor, cc_list_t *attributes)
{
	char *obj_ref = cc_calvinsys_open(*actor, "media.camerahandler", NULL);

	if (obj_ref == NULL) {
		cc_log_error("'media.camerahandler' not supported");
		return CC_FAIL;
	}

	(*actor)->instance_state = (void *)obj_ref;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_camera_set_state(cc_actor_t **actor, cc_list_t *attributes)
{
	return cc_actor_camera_init(actor, attributes);
}

static bool cc_actor_camera_fire(struct cc_actor_t *actor)
{
	if (actor->instance_state != NULL) {
		char *obj_ref = (char *)actor->instance_state;
		cc_port_t *inport = (cc_port_t *)actor->in_ports->data, *outport = (cc_port_t *)actor->out_ports->data;

		if (obj_ref != NULL && cc_fifo_tokens_available(inport->fifo, 1)) {
			cc_fifo_peek(inport->fifo);
			if (cc_calvinsys_write(actor->calvinsys, obj_ref, (char *)"trigger", 7) != CC_SUCCESS) {
				return false;
			} else {
				cc_fifo_commit_read(inport->fifo, false);
				return true;
			}
		} else if (obj_ref != NULL && cc_calvinsys_can_read(actor->calvinsys, obj_ref) == true) {
			char* sys_data;
			size_t sys_data_size;
			cc_calvinsys_read(actor->calvinsys, obj_ref, &sys_data, &sys_data_size);
			cc_log_debug("Read data from camera sys with size %d!", sys_data_size);
			if (cc_fifo_slots_available(outport->fifo, 1)) {
				if (cc_fifo_write(outport->fifo, sys_data, sys_data_size) != CC_SUCCESS) {
					cc_log_error("Failed to send camera data");
					return false;
				}
				cc_log_debug("wrote data to port!");
				return true;
			} else
				cc_log_error("no slots available on outport!");
		}
	}
	return false;
}

cc_result_t cc_actor_camera_get_requires(cc_actor_t *actor, cc_list_t **requires)
{
	if (cc_list_add_n(requires, "media.camerahandler", 19, NULL, 0) == NULL) {
		cc_log_error("Failed to add requires");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_camera_setup(cc_actor_type_t *type)
{
	type->init = cc_actor_camera_init;
	type->set_state = cc_actor_camera_set_state;
	type->fire_actor = cc_actor_camera_fire;
	type->get_requires = cc_actor_camera_get_requires;

	return CC_SUCCESS;
}
