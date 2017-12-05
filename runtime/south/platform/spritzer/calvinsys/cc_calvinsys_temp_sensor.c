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
#include "cc_calvinsys_temp_sensor.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/coder/cc_coder.h"
#include "runtime/south/platform/cc_platform.h"
#include "calvinsys/cc_calvinsys.h"

static bool cc_calvinsys_temp_sensor_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t cc_calvinsys_temp_sensor_write(cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	cc_calvinsys_temperature_state_t *state = (cc_calvinsys_temperature_state_t *)obj->capability->state;

	if (cc_coder_decode_bool(data, &state->can_read) != CC_SUCCESS) {
		cc_log_error("Failed to decode value");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static bool cc_calvinsys_temp_sensor_can_read(struct cc_calvinsys_obj_t *obj)
{
	return ((cc_calvinsys_temperature_state_t *)obj->capability->state)->can_read;
}

static cc_result_t cc_calvinsys_temp_sensor_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	cc_calvinsys_temperature_state_t *state = (cc_calvinsys_temperature_state_t *)obj->capability->state;

	if (!state->can_read)
		return CC_FAIL;

	*size = cc_coder_sizeof_double(state->temp);
	if (cc_platform_mem_alloc((void **)data, *size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	cc_coder_encode_double(*data, state->temp);

	state->can_read = false;

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_temp_sensor_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	obj->can_write = cc_calvinsys_temp_sensor_can_write;
	obj->write = cc_calvinsys_temp_sensor_write;
	obj->can_read = cc_calvinsys_temp_sensor_can_read;
	obj->read = cc_calvinsys_temp_sensor_read;

	return CC_SUCCESS;
}
