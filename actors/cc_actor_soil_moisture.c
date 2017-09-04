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
#include "cc_actor_soil_moisture.h"
#include "../runtime/north/cc_actor_store.h"
#include "../runtime/north/cc_fifo.h"
#include "../runtime/north/cc_token.h"
#include "../runtime/north/cc_port.h"
#include "../runtime/north/cc_common.h"
#include "../calvinsys/common/cc_calvinsys_timer.h"
#include "../msgpuck/msgpuck.h"

typedef struct actor_soil_moisture_state_t {
	calvinsys_obj_t *soilmoisture;
	calvinsys_obj_t *timer;
} actor_soil_moisture_state_t;

static result_t actor_soil_moisture_init(actor_t **actor, list_t *attributes)
{
	actor_soil_moisture_state_t *state = NULL;
	char *data = NULL;
	list_t *tmp = attributes;
	uint32_t data_len = 0;

	while (tmp != NULL) {
		if (strncmp(tmp->id, "frequency", 9) == 0) {
			data = (char *)tmp->data;
			break;
		}
		tmp = tmp->next;
	}

	if (data == NULL) {
		cc_log_error("Failed to get attribute 'frequency'");
		return CC_RESULT_FAIL;
	}

	if (platform_mem_alloc((void **)&state, sizeof(actor_soil_moisture_state_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	state->soilmoisture = calvinsys_open((*actor)->calvinsys, "io.soilmoisture", NULL, 0);
	if (state->soilmoisture == NULL) {
		cc_log_error("Failed to open 'io.soilmoisture'");
		return CC_RESULT_FAIL;
	}

	state->timer = calvinsys_open((*actor)->calvinsys, "sys.timer.once", NULL, 0);
	if (state->timer == NULL) {
		cc_log_error("Failed to open 'sys.timer.once'");
		return CC_RESULT_FAIL;
	}

	state->timer->write(state->timer, data, data_len);

	(*actor)->instance_state = (void *)state;

	return CC_RESULT_SUCCESS;
}

static result_t actor_soil_moisture_set_state(actor_t **actor, list_t *attributes)
{
	return actor_soil_moisture_init(actor, attributes);
}

static bool actor_soil_moisture_fire(struct actor_t *actor)
{
	port_t *outport = (port_t *)actor->out_ports->data;
	actor_soil_moisture_state_t *state = (actor_soil_moisture_state_t *)actor->instance_state;
	calvinsys_obj_t *obj_soilmoisture = state->soilmoisture;
	calvinsys_obj_t *obj_timer = state->timer;
	char *data = NULL;
	size_t size = 0;

	if (obj_timer->can_read(obj_timer) && fifo_slots_available(outport->fifo, 1)) {
		if (obj_soilmoisture->read(obj_soilmoisture, &data, &size) == CC_RESULT_SUCCESS) {
			if (fifo_write(outport->fifo, data, size) == CC_RESULT_SUCCESS) {
				size = mp_sizeof_bool(true);
				if (platform_mem_alloc((void **)&data, size) == CC_RESULT_SUCCESS) {
					obj_timer->read(obj_timer, &data, &size);
					mp_encode_bool(data, true);
					obj_timer->write(obj_timer, data, size);
					platform_mem_free((void *)data);
					return true;
				} else
					cc_log_error("Failed to allocate memory");
			}
			platform_mem_free((void *)data);
		} else
			cc_log_error("Failed to read value");
	}

	return false;
}

static void actor_soil_moisture_free(actor_t *actor)
{
	calvinsys_close((calvinsys_obj_t *)actor->instance_state);
}

static result_t actor_soil_moisture_managed_attributes(actor_t *actor, list_t **attributes)
{
	uint32_t size = 0;
	char *value = NULL, *name = NULL;
	actor_soil_moisture_state_t *state = (actor_soil_moisture_state_t *)actor->instance_state;

	if (platform_mem_alloc((void **)&name, strlen("frequency") + 1) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}
	strcpy(name, "frequency");

	size = mp_sizeof_uint(((calvinsys_timer_t *)state->timer->state)->timeout);
	if (platform_mem_alloc((void **)&value, size) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	mp_encode_uint(value, ((calvinsys_timer_t *)state->timer->state)->timeout);

	if (list_add(attributes, name, value, size) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to add '%s' to managed list", name);
		platform_mem_free(name);
		platform_mem_free(value);
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

result_t actor_soil_moisture_register(list_t **actor_types)
{
	actor_type_t *type = NULL;

	if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	type->init = actor_soil_moisture_init;
	type->set_state = actor_soil_moisture_set_state;
	type->free_state = actor_soil_moisture_free;
	type->fire_actor = actor_soil_moisture_fire;
	type->get_managed_attributes = actor_soil_moisture_managed_attributes;
	type->will_migrate = NULL;
	type->will_end = NULL;
	type->did_migrate = NULL;

	return list_add_n(actor_types, "sensor.SoilMoisture", 19, type, sizeof(actor_type_t *));
}
