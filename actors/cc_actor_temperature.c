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
#include "cc_actor_temperature.h"
#include "../runtime/north/cc_actor_store.h"
#include "../runtime/north/cc_fifo.h"
#include "../runtime/north/cc_token.h"
#include "../runtime/north/cc_port.h"
#include "../runtime/north/cc_common.h"
#include "../runtime/north/coder/cc_coder.h"
#include "../calvinsys/common/cc_calvinsys_timer.h"

typedef struct cc_actor_temperature_state_t {
	cc_calvinsys_obj_t *temperature;
	cc_calvinsys_obj_t *timer;
} cc_actor_temperature_state_t;

static cc_result_t cc_actor_temperature_init(cc_actor_t**actor, cc_list_t *attributes)
{
	cc_actor_temperature_state_t *state = NULL;
	char buffer[50], *data_frequency = NULL, *data_last_triggered = NULL;
	char *buffer_pos = buffer;
	uint32_t timeout = 0, last_triggered = 0;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_temperature_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	data_frequency = (char *)cc_list_get(attributes, "frequency");
	if (data_frequency == NULL) {
		cc_log_error("Failed to get attribute 'frequency'");
		cc_platform_mem_free((void *)state);
		return CC_FAIL;
	}

	switch (cc_coder_type_of(data_frequency)) {
	case CC_CODER_UINT:
	{
		uint32_t frequency;
		cc_coder_decode_uint(data_frequency, &frequency);
		timeout = 1 / frequency;
		break;
	}
	case CC_CODER_FLOAT:
	{
		float frequency;
		cc_coder_decode_float(data_frequency, &frequency);
		timeout = (uint32_t)(1 / frequency);
		break;
	}
	case CC_CODER_DOUBLE:
	{
		double frequency;
		cc_coder_decode_double(data_frequency, &frequency);
		timeout = (uint32_t)(1 / frequency);
		break;
	}
	default:
		cc_log_error("Unknown type");
		cc_platform_mem_free((void *)state);
		return CC_FAIL;
	}

	data_last_triggered = (char *)cc_list_get(attributes, "last_triggered");
	if (data_last_triggered != NULL) {
		cc_coder_decode_uint(data_last_triggered, &last_triggered);
		buffer_pos = cc_coder_encode_map(buffer_pos, 2);
		{
			buffer_pos = cc_coder_encode_kv_uint(buffer_pos, "timeout", timeout);
			buffer_pos = cc_coder_encode_kv_uint(buffer_pos, "last_triggered", last_triggered);
		}
	} else {
		buffer_pos = cc_coder_encode_map(buffer_pos, 1);
		{
			buffer_pos = cc_coder_encode_kv_uint(buffer_pos, "timeout", timeout);
		}
	}

	state->temperature = cc_calvinsys_open((*actor)->calvinsys, "io.temperature", NULL, 0);
	if (state->temperature == NULL) {
		cc_log_error("Failed to open 'io.temperature'");
		cc_platform_mem_free((void *)state);
		return CC_FAIL;
	}

	state->timer = cc_calvinsys_open((*actor)->calvinsys, "sys.timer.once", buffer, buffer_pos - buffer);
	if (state->timer == NULL) {
		cc_log_error("Failed to open 'sys.timer.once'");
		return CC_FAIL;
	}

	(*actor)->instance_state = (void *)state;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_temperature_set_state(cc_actor_t**actor, cc_list_t *attributes)
{
	return cc_actor_temperature_init(actor, attributes);
}

static bool cc_actor_temperature_fire(struct cc_actor_t*actor)
{
	cc_port_t *outport = (cc_port_t *)actor->out_ports->data;
	cc_actor_temperature_state_t *state = (cc_actor_temperature_state_t *)actor->instance_state;
	cc_calvinsys_obj_t *obj_temp = state->temperature;
	cc_calvinsys_obj_t *obj_timer = state->timer;
	char *data = NULL;
	size_t size = 0;

	if (obj_timer->can_read(obj_timer) && obj_temp->can_read(obj_temp) && cc_fifo_slots_available(outport->fifo, 1)) {
		if (obj_temp->read(obj_temp, &data, &size) == CC_SUCCESS) {
			if (cc_fifo_write(outport->fifo, data, size) == CC_SUCCESS) {
				size = cc_coder_sizeof_bool(true);
				if (cc_platform_mem_alloc((void **)&data, size) == CC_SUCCESS) {
					obj_timer->read(obj_timer, &data, &size);
					cc_coder_encode_bool(data, true);
					obj_timer->write(obj_timer, data, size);
					cc_platform_mem_free((void *)data);
					return true;
				} else
					cc_log_error("Failed to allocate memory");
			} else
				cc_log_error("Failed to write to outport");
			cc_platform_mem_free((void *)data);
		} else
			cc_log_error("Failed to read temperature");
	}

	return false;
}

static void cc_actor_temperature_free(cc_actor_t*actor)
{
	cc_actor_temperature_state_t *state = (cc_actor_temperature_state_t *)actor->instance_state;

	cc_calvinsys_close(state->temperature);
	cc_calvinsys_close(state->timer);

	cc_platform_mem_free((void *)state);
}

static cc_result_t cc_actor_temperature_add_last_triggered(cc_calvinsys_obj_t *obj, cc_list_t **attributes)
{
	uint32_t size = 0;
	char *value = NULL, *name = NULL;
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;

	if (!timer->active)
		return CC_SUCCESS;

	if (cc_platform_mem_alloc((void **)&name, 15) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	strncpy(name, "last_triggered", 14);
	name[13] = '\0';

	size = cc_coder_sizeof_uint(timer->last_triggered);
	if (cc_platform_mem_alloc((void **)&value, size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		cc_platform_mem_free((void *)name);
		return CC_FAIL;
	}

	cc_coder_encode_uint(value, timer->last_triggered);

	if (cc_list_add(attributes, name, value, size) != CC_SUCCESS) {
		cc_log_error("Failed to add '%s' to managed list", name);
		cc_platform_mem_free(name);
		cc_platform_mem_free(value);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static cc_result_t cc_actor_temperature_get_managed_attributes(cc_actor_t*actor, cc_list_t **attributes)
{
	uint32_t size = 0;
	char *value = NULL, *name = NULL;
	cc_actor_temperature_state_t *state = (cc_actor_temperature_state_t *)actor->instance_state;
	cc_calvinsys_obj_t *timer_obj = state->timer;
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)timer_obj->state;
	double frequency;

	if (!timer->active || timer->timeout == 0)
		return CC_SUCCESS;

	if (cc_platform_mem_alloc((void **)&name, 10) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	strncpy(name, "frequency", 9);
	name[9] = '\0';

	frequency = 1.0 / timer->timeout;

	size = cc_coder_sizeof_double(frequency);
	if (cc_platform_mem_alloc((void **)&value, size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	cc_coder_encode_double(value, frequency);

	if (cc_list_add(attributes, name, value, size) != CC_SUCCESS) {
		cc_log_error("Failed to add '%s' to managed list", name);
		cc_platform_mem_free(name);
		cc_platform_mem_free(value);
		return CC_FAIL;
	}

	return cc_actor_temperature_add_last_triggered(timer_obj, attributes);
}

cc_result_t cc_actor_temperature_register(cc_list_t **actor_types)
{
	cc_actor_type_t *type = NULL;

	if (cc_platform_mem_alloc((void **)&type, sizeof(cc_actor_type_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	type->init = cc_actor_temperature_init;
	type->set_state = cc_actor_temperature_set_state;
	type->free_state = cc_actor_temperature_free;
	type->fire_actor = cc_actor_temperature_fire;
	type->get_managed_attributes = cc_actor_temperature_get_managed_attributes;
	type->will_migrate = NULL;
	type->will_end = NULL;
	type->did_migrate = NULL;

	return cc_list_add_n(actor_types, "sensor.Temperature", 18, type, sizeof(cc_actor_type_t *));
}
