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
#include "../runtime/north/cc_msgpack_helper.h"
#include "../calvinsys/common/cc_calvinsys_timer.h"
#include "../msgpuck/msgpuck.h"

typedef struct actor_temperature_state_t {
	calvinsys_obj_t *temperature;
	calvinsys_obj_t *timer;
} actor_temperature_state_t;

static result_t actor_temperature_init(actor_t **actor, list_t *attributes)
{
	actor_temperature_state_t *state = NULL;
	char buffer[50], *data = NULL;
	char *buffer_pos = buffer;
	uint32_t timeout = 0, last_triggered = 0;

	if (platform_mem_alloc((void **)&state, sizeof(actor_temperature_state_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	data = (char *)list_get(attributes, "frequency");
	if (data == NULL) {
		cc_log_error("Failed to get attribute 'frequency'");
		platform_mem_free((void *)state);
		return CC_RESULT_FAIL;
	}

	switch (mp_typeof(*data)) {
	case MP_UINT:
	{
		uint32_t frequency;
		decode_uint(data, &frequency);
		timeout = 1 / frequency;
		break;
	}
	case MP_FLOAT:
	{
		float frequency;
		decode_float(data, &frequency);
		timeout = 1 / (uint32_t)frequency;
		break;
	}
	case MP_DOUBLE:
	{
		double frequency;
		decode_double(data, &frequency);
		timeout = (uint32_t)(1 / frequency);
		break;
	}
	default:
		cc_log_error("Unknown type");
		platform_mem_free((void *)state);
		return CC_RESULT_FAIL;
	}

	data = (char *)list_get(attributes, "last_triggered");
	if (data != NULL) {
		decode_uint(data, &last_triggered);
		buffer_pos = mp_encode_map(buffer_pos, 2);
		{
			buffer_pos = encode_uint(&buffer_pos, "timeout", timeout);
			buffer_pos = encode_uint(&buffer_pos, "last_triggered", last_triggered);
		}
	} else {
		buffer_pos = mp_encode_map(buffer_pos, 1);
		{
			buffer_pos = encode_uint(&buffer_pos, "timeout", timeout);
		}
	}

	state->temperature = calvinsys_open((*actor)->calvinsys, "io.temperature", NULL, 0);
	if (state->temperature == NULL) {
		cc_log_error("Failed to open 'io.temperature'");
		platform_mem_free((void *)state);
		return CC_RESULT_FAIL;
	}

	state->timer = calvinsys_open((*actor)->calvinsys, "sys.timer.once", buffer, buffer_pos - buffer);
	if (state->timer == NULL) {
		cc_log_error("Failed to open 'sys.timer.once'");
		return CC_RESULT_FAIL;
	}

	(*actor)->instance_state = (void *)state;

	return CC_RESULT_SUCCESS;
}

static result_t actor_temperature_set_state(actor_t **actor, list_t *attributes)
{
	return actor_temperature_init(actor, attributes);
}

static bool actor_temperature_fire(struct actor_t *actor)
{
	port_t *outport = (port_t *)actor->out_ports->data;
	actor_temperature_state_t *state = (actor_temperature_state_t *)actor->instance_state;
	calvinsys_obj_t *obj_temp = state->temperature;
	calvinsys_obj_t *obj_timer = state->timer;
	char *data = NULL;
	size_t size = 0;

	if (obj_timer->can_read(obj_timer) && obj_temp->can_read(obj_temp) && fifo_slots_available(outport->fifo, 1)) {
		if (obj_temp->read(obj_temp, &data, &size) == CC_RESULT_SUCCESS) {
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
			} else
				cc_log_error("Failed to write to outport");
			platform_mem_free((void *)data);
		} else
			cc_log_error("Failed to read temperature");
	}

	return false;
}

static void actor_temperature_free(actor_t *actor)
{
	actor_temperature_state_t *state = (actor_temperature_state_t *)actor->instance_state;

	calvinsys_close(state->temperature);
	calvinsys_close(state->timer);
}

static result_t actor_temperature_add_last_triggered(calvinsys_obj_t *obj, list_t **attributes)
{
	uint32_t size = 0;
	char *value = NULL, *name = NULL;
	calvinsys_timer_t *timer = (calvinsys_timer_t *)obj->state;

	if (!timer->active)
		return CC_RESULT_SUCCESS;

	if (platform_mem_alloc((void **)&name, strlen("last_triggered") + 1) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}
	strcpy(name, "last_triggered");

	size = mp_sizeof_uint(timer->last_triggered);
	if (platform_mem_alloc((void **)&value, size) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		platform_mem_free((void *)name);
		return CC_RESULT_FAIL;
	}

	mp_encode_uint(value, timer->last_triggered);

	if (list_add(attributes, name, value, size) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to add '%s' to managed list", name);
		platform_mem_free(name);
		platform_mem_free(value);
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

static result_t actor_temperature_get_managed_attributes(actor_t *actor, list_t **attributes)
{
	uint32_t size = 0;
	char *value = NULL, *name = NULL;
	actor_temperature_state_t *state = (actor_temperature_state_t *)actor->instance_state;
	calvinsys_obj_t *timer_obj = state->timer;
	calvinsys_timer_t *timer = (calvinsys_timer_t *)timer_obj->state;
	double frequency;

	if (!timer->active || timer->timeout == 0)
		return CC_RESULT_SUCCESS;

	if (platform_mem_alloc((void **)&name, strlen("frequency") + 1) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}
	strcpy(name, "frequency");

	frequency = 1.0 / timer->timeout;

	size = mp_sizeof_double(frequency);
	if (platform_mem_alloc((void **)&value, size) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}
	mp_encode_double(value, frequency);

	if (list_add(attributes, name, value, size) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to add '%s' to managed list", name);
		platform_mem_free(name);
		platform_mem_free(value);
		return CC_RESULT_FAIL;
	}

	return actor_temperature_add_last_triggered(timer_obj, attributes);
}

result_t actor_temperature_register(list_t **actor_types)
{
	actor_type_t *type = NULL;

	if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	type->init = actor_temperature_init;
	type->set_state = actor_temperature_set_state;
	type->free_state = actor_temperature_free;
	type->fire_actor = actor_temperature_fire;
	type->get_managed_attributes = actor_temperature_get_managed_attributes;
	type->will_migrate = NULL;
	type->will_end = NULL;
	type->did_migrate = NULL;

	return list_add_n(actor_types, "sensor.Temperature", strlen("sensor.Temperature"), type, sizeof(actor_type_t *));
}
