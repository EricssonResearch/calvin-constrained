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
#include "runtime/north/cc_node.h"
#include "runtime/north/coder/cc_coder.h"
#include "runtime/south/platform/cc_platform.h"
#include "calvinsys/cc_calvinsys.h"
#include "ds18b20/ds18b20.h"

typedef struct cc_calvinsys_temperature_state_t {
	uint32_t pin;
	bool can_read;
	float temp;
} cc_calvinsys_temperature_state_t;

static bool cc_calvinsys_ds18b20_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t cc_calvinsys_ds18b20_write(cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	int nsensors = 0;
	ds18b20_addr_t addrs[1];
	cc_calvinsys_temperature_state_t *state = (cc_calvinsys_temperature_state_t *)obj->state;

	if (cc_coder_decode_bool(data, &state->can_read) != CC_SUCCESS) {
		cc_log_error("Failed to decode value");
		state->can_read = false;
		return CC_FAIL;
	}

	if (state->can_read) {
		nsensors = ds18b20_scan_devices(state->pin, addrs, 1);
		if (nsensors < 1) {
			cc_log_error("Failed to scan devices, count '%d'", nsensors);
			return CC_FAIL;
		}
		state->temp = ds18b20_measure_and_read(state->pin, addrs[0]);
	}

	return CC_SUCCESS;
}

static bool cc_calvinsys_ds18b20_can_read(struct cc_calvinsys_obj_t *obj)
{
	return ((cc_calvinsys_temperature_state_t *)obj->state)->can_read;
}

static cc_result_t cc_calvinsys_ds18b20_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	char *w = NULL;
	cc_calvinsys_temperature_state_t *state = (cc_calvinsys_temperature_state_t *)obj->state;

	*size = cc_coder_sizeof_float(state->temp);
	if (cc_platform_mem_alloc((void **)data, *size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	w = *data;
	w = cc_coder_encode_float(w, state->temp);

	state->can_read = false;

	return CC_SUCCESS;
}

static cc_result_t cc_calvinsys_ds18b20_close(struct cc_calvinsys_obj_t *obj)
{
	cc_platform_mem_free(obj->state);

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_ds18b20_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	char *init_args = (char *)obj->capability->init_args;
	cc_calvinsys_temperature_state_t *state = NULL;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_calvinsys_temperature_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(init_args, "pin", &state->pin) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'pin'");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	state->can_read = false;
	obj->can_write = cc_calvinsys_ds18b20_can_write;
	obj->write = cc_calvinsys_ds18b20_write;
	obj->can_read = cc_calvinsys_ds18b20_can_read;
	obj->read = cc_calvinsys_ds18b20_read;
	obj->close = cc_calvinsys_ds18b20_close;
	obj->state = state;

	return CC_SUCCESS;
}
