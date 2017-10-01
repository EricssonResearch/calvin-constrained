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
#include "../calvinsys/cc_calvinsys.h"

typedef struct cc_actor_temperature_state_t {
	char temperature[CC_UUID_BUFFER_SIZE];
	char timer[CC_UUID_BUFFER_SIZE];
} cc_actor_temperature_state_t;

static cc_result_t cc_actor_temperature_init(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	cc_actor_temperature_state_t *state = NULL;
	char buffer[50], *data_frequency = NULL, *buffer_pos = buffer;
	uint32_t timeout = 0;
	cc_list_t *item = NULL;
	char *obj_ref = NULL;
	uint32_t obj_ref_len = 0;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_temperature_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	item = cc_list_get(managed_attributes, "temperature");
	if (item != NULL) {
		if (cc_coder_decode_str((char *)item->data, &obj_ref, &obj_ref_len) != CC_SUCCESS) {
			cc_log_error("Failed to decode 'temperature'");
			cc_platform_mem_free(state);
			return CC_FAIL;
		}
		strncpy(state->temperature, obj_ref, strnlen(obj_ref, CC_UUID_BUFFER_SIZE));

		item = cc_list_get(managed_attributes, "timer");
		if (item != NULL) {
			if (cc_coder_decode_str((char *)item->data, &obj_ref, &obj_ref_len) != CC_SUCCESS) {
				cc_log_error("Failed to decode 'timer'");
				cc_platform_mem_free(state);
				return CC_FAIL;
			}
			strncpy(state->timer, obj_ref, strnlen(obj_ref, CC_UUID_BUFFER_SIZE));
		} else {
			cc_log_error("Failed to get 'timer'");
			return CC_FAIL;
		}

		(*actor)->instance_state = (void *)state;
		return CC_SUCCESS;
	}

	// No state, create from attributes
	obj_ref = cc_calvinsys_open(*actor, "io.temperature", NULL, 0);
	if (obj_ref == NULL) {
		cc_log_error("Failed to open 'io.temperature'");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}
	strncpy(state->temperature, obj_ref, strnlen(obj_ref, CC_UUID_BUFFER_SIZE));

	item = cc_list_get(managed_attributes, "frequency");
	if (item == NULL) {
		cc_log_error("Failed to get attribute 'frequency'");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	data_frequency = (char *)item->data;

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

	buffer_pos = cc_coder_encode_uint(buffer_pos, timeout);
	obj_ref = cc_calvinsys_open(*actor, "sys.timer.once", buffer, buffer_pos - buffer);
	if (obj_ref == NULL) {
		cc_log_error("Failed to open 'sys.timer.once'");
		cc_platform_mem_free((void *)state);
		return CC_FAIL;
	}
	strncpy(state->timer, obj_ref, strnlen(obj_ref, CC_UUID_BUFFER_SIZE));

	(*actor)->instance_state = (void *)state;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_temperature_set_state(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	return cc_actor_temperature_init(actor, managed_attributes);
}

static bool cc_actor_temperature_fire(struct cc_actor_t *actor)
{
	cc_port_t *outport = (cc_port_t *)actor->out_ports->data;
	cc_actor_temperature_state_t *state = (cc_actor_temperature_state_t *)actor->instance_state;
	char *data_temp = NULL, *data_bool = NULL;
	size_t size = 0;

	if (!cc_calvinsys_can_read(actor->calvinsys, state->timer) || !cc_calvinsys_can_read(actor->calvinsys, state->temperature))
	 	return false;

	if (!cc_fifo_slots_available(outport->fifo, 1))
		return false;

	size = cc_coder_sizeof_bool(true);
	if (cc_platform_mem_alloc((void **)&data_bool, size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return false;
	}

	if (cc_coder_encode_bool(data_bool, true) == NULL) {
		cc_log_error("Failed to encode value");
		cc_platform_mem_free((void *)data_bool);
		return false;
	}

	if (cc_calvinsys_write(actor->calvinsys, state->timer, data_bool, size) != CC_SUCCESS) {
		cc_log_error("Failed to set timer");
		cc_platform_mem_free((void *)data_bool);
		return false;
	}

	cc_platform_mem_free((void *)data_bool);

	if (cc_calvinsys_read(actor->calvinsys, state->temperature, &data_temp, &size) != CC_SUCCESS)
		return false;

	if (cc_fifo_write(outport->fifo, data_temp, size) != CC_SUCCESS) {
		cc_platform_mem_free((void *)data_temp);
		return false;
	}

	return true;
}

static void cc_actor_temperature_free(cc_actor_t *actor)
{
	if (actor->instance_state != NULL)
		cc_platform_mem_free((void *)actor->instance_state);
}

static cc_result_t cc_actor_temperature_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	cc_actor_temperature_state_t *state = (cc_actor_temperature_state_t *)actor->instance_state;
	char *buffer = NULL, *w = NULL;

	if (cc_platform_mem_alloc((void **)&buffer, cc_coder_sizeof_str(strlen(state->timer))) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	w = buffer;
	w = cc_coder_encode_str(w, state->timer, strlen(state->timer));

	if (cc_list_add_n(managed_attributes, "timer", 5, buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add 'timer' to managed attributes");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&buffer, cc_coder_sizeof_str(strlen(state->temperature))) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	w = buffer;
	w = cc_coder_encode_str(w, state->temperature, strlen(state->temperature));

	if (cc_list_add_n(managed_attributes, "temperature", 5, buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add 'temperature' to managed attributes");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}


	return CC_SUCCESS;
}

cc_result_t cc_actor_temperature_register(cc_list_t **actor_types)
{
	cc_actor_type_t *type = NULL;

	if (cc_platform_mem_alloc((void **)&type, sizeof(cc_actor_type_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(type, 0, sizeof(cc_actor_type_t));
	type->init = cc_actor_temperature_init;
	type->set_state = cc_actor_temperature_set_state;
	type->free_state = cc_actor_temperature_free;
	type->fire_actor = cc_actor_temperature_fire;
	type->get_managed_attributes = cc_actor_temperature_get_attributes;

	if (cc_list_add_n(actor_types, "sensor.Temperature", 18, type, sizeof(cc_actor_type_t *)) == NULL) {
		cc_log_error("Failed to register 'sensor.Temperature'");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}
