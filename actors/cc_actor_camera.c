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
#include "cc_actor_camera.h"
#include "../runtime/north/cc_actor.h"
#include "../runtime/north/cc_actor_store.h"

static result_t actor_camera_init(actor_t **actor, list_t *attributes)
{
	calvinsys_obj_t* camera = calvinsys_open((*actor)->calvinsys, "media.camerahandler", NULL, 0);

	if (camera == NULL) {
		cc_log_error("media.camerahandler not supported");
		return CC_RESULT_FAIL;
	}

	(*actor)->instance_state = (void *)camera;
	return CC_RESULT_SUCCESS;
}

static result_t actor_camera_set_state(actor_t **actor, list_t *attributes)
{
	return actor_camera_init(actor, attributes);
}

static bool actor_camera_fire(struct actor_t *actor)
{
	if (actor->instance_state != NULL) {
		calvinsys_obj_t *camera = (calvinsys_obj_t *)actor->instance_state;
		port_t *inport = (port_t *)actor->in_ports->data, *outport = (port_t *)actor->out_ports->data;

		if(camera != NULL && fifo_tokens_available(inport->fifo, 1)) {
			fifo_peek(inport->fifo);
			if (camera->write(camera, "trigger", 7) != CC_RESULT_SUCCESS) {
				return false;
			} else {
				fifo_commit_read(inport->fifo, false);
				return true;
			}
		} else if (camera != NULL && camera->can_read(camera) == true) {
			char* sys_data;
			size_t sys_data_size;
			camera->read(camera, &sys_data, &sys_data_size);
			cc_log_debug("Read data from camera sys with size %d!", sys_data_size);
			if (fifo_slots_available(outport->fifo, 1)) {
				if (fifo_write(outport->fifo, sys_data, sys_data_size) != CC_RESULT_SUCCESS) {
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

void actor_camera_free(actor_t *actor)
{
	calvinsys_close((calvinsys_obj_t *)actor->instance_state);
}

result_t actor_camera_register(list_t **actor_types)
{
	actor_type_t *type = NULL;

	if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	type->init = actor_camera_init;
	type->set_state = actor_camera_set_state;
	type->free_state = actor_camera_free;
	type->fire_actor = actor_camera_fire;
	type->get_managed_attributes = NULL;
	type->will_migrate = NULL;
	type->will_end = NULL;
	type->did_migrate = NULL;

	return list_add_n(actor_types, "media.Camera", 12, type, sizeof(actor_type_t *));
}
