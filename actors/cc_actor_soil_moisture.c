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

typedef struct cc_actor_soil_moisture_state_t {
	cc_calvinsys_obj_t *soilmoisture;
	cc_calvinsys_obj_t *timer;
} cc_actor_soil_moisture_state_t;

static cc_result_t cc_actor_soil_moisture_init(cc_actor_t**actor, cc_list_t *attributes)
{
	cc_actor_soil_moisture_state_t *state = NULL;
	char *data = NULL;
	cc_list_t *tmp = attributes;
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
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_soil_moisture_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	state->soilmoisture = cc_calvinsys_open((*actor)->calvinsys, "io.soilmoisture", NULL, 0);
	if (state->soilmoisture == NULL) {
		cc_log_error("Failed to open 'io.soilmoisture'");
		return CC_FAIL;
	}

	state->timer = cc_calvinsys_open((*actor)->calvinsys, "sys.timer.once", NULL, 0);
	if (state->timer == NULL) {
		cc_log_error("Failed to open 'sys.timer.once'");
		return CC_FAIL;
	}

	state->timer->write(state->timer, data, data_len);

	(*actor)->instance_state = (void *)state;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_soil_moisture_set_state(cc_actor_t**actor, cc_list_t *attributes)
{
	return cc_actor_soil_moisture_init(actor, attributes);
}

static bool cc_actor_soil_moisture_fire(struct cc_actor_t*actor)
{
	cc_port_t *outport = (cc_port_t *)actor->out_ports->data;
	cc_actor_soil_moisture_state_t *state = (cc_actor_soil_moisture_state_t *)actor->instance_state;
	cc_calvinsys_obj_t *obj_soilmoisture = state->soilmoisture;
	cc_calvinsys_obj_t *obj_timer = state->timer;
	char *data = NULL;
	size_t size = 0;

	if (obj_timer->can_read(obj_timer) && cc_fifo_slots_available(outport->fifo, 1)) {
		if (obj_soilmoisture->read(obj_soilmoisture, &data, &size) == CC_SUCCESS) {
			if (cc_fifo_write(outport->fifo, data, size) == CC_SUCCESS) {
				size = mp_sizeof_bool(true);
				if (cc_platform_mem_alloc((void **)&data, size) == CC_SUCCESS) {
					obj_timer->read(obj_timer, &data, &size);
					mp_encode_bool(data, true);
					obj_timer->write(obj_timer, data, size);
					cc_platform_mem_free((void *)data);
					return true;
				} else
					cc_log_error("Failed to allocate memory");
			}
			cc_platform_mem_free((void *)data);
		} else
			cc_log_error("Failed to read value");
	}

	return false;
}

static void cc_actor_soil_moisture_free(cc_actor_t*actor)
{
	cc_calvinsys_close((cc_calvinsys_obj_t *)actor->instance_state);
}

static cc_result_t cc_actor_soil_moisture_managed_attributes(cc_actor_t*actor, cc_list_t **attributes)
{
	uint32_t size = 0;
	char *value = NULL, *name = NULL;
	cc_actor_soil_moisture_state_t *state = (cc_actor_soil_moisture_state_t *)actor->instance_state;

	if (cc_platform_mem_alloc((void **)&name, 10) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	strncpy(name, "frequency", 9);
	name[9] = '\0';

	size = mp_sizeof_uint(((cc_calvinsys_timer_t *)state->timer->state)->timeout);
	if (cc_platform_mem_alloc((void **)&value, size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	mp_encode_uint(value, ((cc_calvinsys_timer_t *)state->timer->state)->timeout);

	if (cc_list_add(attributes, name, value, size) != CC_SUCCESS) {
		cc_log_error("Failed to add '%s' to managed list", name);
		cc_platform_mem_free(name);
		cc_platform_mem_free(value);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_soil_moisture_register(cc_list_t **actor_types)
{
	cc_actor_type_t *type = NULL;

	if (cc_platform_mem_alloc((void **)&type, sizeof(cc_actor_type_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	type->init = cc_actor_soil_moisture_init;
	type->set_state = cc_actor_soil_moisture_set_state;
	type->free_state = cc_actor_soil_moisture_free;
	type->fire_actor = cc_actor_soil_moisture_fire;
	type->get_managed_attributes = cc_actor_soil_moisture_managed_attributes;
	type->will_migrate = NULL;
	type->will_end = NULL;
	type->did_migrate = NULL;

	return cc_list_add_n(actor_types, "sensor.SoilMoisture", 19, type, sizeof(cc_actor_type_t *));
}
