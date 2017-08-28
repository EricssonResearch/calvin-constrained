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
#include "../../cc_platform.h"
#include "../../../../../calvinsys/cc_calvinsys.h"
#include "../../../../../msgpuck/msgpuck.h"
#include "ds18b20/ds18b20.h"

#define CC_DS18B20_SENSOR_GPIO 5

typedef struct calvinsys_ds18b20_state_t {
	ds18b20_addr_t addr;
} calvinsys_ds18b20_state_t;

static bool calvinsys_ds18b20_can_read(struct calvinsys_obj_t *obj)
{
	return true;
}

static result_t calvinsys_ds18b20_read(struct calvinsys_obj_t *obj, char **data, size_t *size)
{
	float temp;
	calvinsys_ds18b20_state_t *state = (calvinsys_ds18b20_state_t *)obj->state;

	temp = ds18b20_measure_and_read(CC_DS18B20_SENSOR_GPIO, state->addr);

	*size = mp_sizeof_float(temp);
	if (platform_mem_alloc((void **)data, *size) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}
	mp_encode_float(*data, temp);

	return CC_RESULT_SUCCESS;
}

static result_t calvinsys_ds18b20_close(struct calvinsys_obj_t *obj)
{
	platform_mem_free((void *)obj->state);
	return CC_RESULT_SUCCESS;
}

static calvinsys_obj_t *calvinsys_ds18b20_open(calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char *capability_name)
{
	calvinsys_obj_t *obj = NULL;
	int sensor_count = 0;
	calvinsys_ds18b20_state_t *ds18b20_state = NULL;

	if (platform_mem_alloc((void **)&ds18b20_state, sizeof(calvinsys_ds18b20_state_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	gpio_set_pullup(CC_DS18B20_SENSOR_GPIO, true, true);

	sensor_count = ds18b20_scan_devices(CC_DS18B20_SENSOR_GPIO, &ds18b20_state->addr, 1);
	if (sensor_count != 1) {
		cc_log_error("No sensor detected");
		platform_mem_free((void *)ds18b20_state);
		return NULL;
	}

	if (platform_mem_alloc((void **)&obj, sizeof(calvinsys_obj_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		platform_mem_free((void *)ds18b20_state);
		return NULL;
	}

	obj->can_write = NULL;
	obj->write = NULL;
	obj->can_read = calvinsys_ds18b20_can_read;
	obj->read = calvinsys_ds18b20_read;
	obj->close = calvinsys_ds18b20_close;
	obj->handler = handler;
	obj->next = NULL;
	obj->state = ds18b20_state;
	handler->objects = obj; // assume only one object

	return obj;
}

result_t calvinsys_ds18b20_create(calvinsys_t **calvinsys, const char *name)
{
	calvinsys_handler_t *handler = NULL;

	if (platform_mem_alloc((void **)&handler, sizeof(calvinsys_handler_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	handler->open = calvinsys_ds18b20_open;
	handler->objects = NULL;
	handler->next = NULL;

	calvinsys_add_handler(calvinsys, handler);
	if (calvinsys_register_capability(*calvinsys, name, handler, NULL) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	return CC_RESULT_SUCCESS;
}
