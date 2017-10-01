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
#include "cc_calvinsys_ds18b20.h"
#include "../../../../north/cc_node.h"
#include "../../../../north/coder/cc_coder.h"
#include "../../cc_platform.h"
#include "../../../../../calvinsys/cc_calvinsys.h"
#include "ds18b20/ds18b20.h"

#define CC_DS18B20_SENSOR_GPIO 5

typedef struct cc_calvinsys_ds18b20_state_t {
	ds18b20_addr_t addr;
} cc_calvinsys_ds18b20_state_t;

static bool cc_calvinsys_ds18b20_can_read(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t cc_calvinsys_ds18b20_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	float temp;
	cc_calvinsys_ds18b20_state_t *state = (cc_calvinsys_ds18b20_state_t *)obj->state;

	temp = ds18b20_measure_and_read(CC_DS18B20_SENSOR_GPIO, state->addr);

	*size = cc_coder_sizeof_float(temp);
	if (cc_platform_mem_alloc((void **)data, *size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	*data = cc_coder_encode_float(*data, temp);

	return CC_SUCCESS;
}

static cc_result_t cc_calvinsys_ds18b20_close(struct cc_calvinsys_obj_t *obj)
{
	cc_platform_mem_free((void *)obj->state);
	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_ds18b20_open(cc_calvinsys_obj_t *obj, char *data, size_t len)
{
	int sensor_count = 0;
	cc_calvinsys_ds18b20_state_t *state = NULL;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_calvinsys_ds18b20_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	gpio_set_pullup(CC_DS18B20_SENSOR_GPIO, true, true);

	sensor_count = ds18b20_scan_devices(CC_DS18B20_SENSOR_GPIO, &state->addr, 1);
	if (sensor_count != 1) {
		cc_log_error("No sensor detected");
		cc_platform_mem_free((void *)state);
		return CC_FAIL;
	}

	obj->can_read = cc_calvinsys_ds18b20_can_read;
	obj->read = cc_calvinsys_ds18b20_read;
	obj->close = cc_calvinsys_ds18b20_close;
	obj->state = state;

	return CC_SUCCESS;
}
