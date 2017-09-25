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
#include "../../../../north/cc_node.h"
#include "../../../../north/coder/cc_coder.h"
#include "../../cc_platform.h"
#include "../../../../../calvinsys/cc_calvinsys.h"

static bool cc_calvinsys_temp_sensor_can_read(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t cc_calvinsys_temp_sensor_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	float temp = 0.0;

	// TODO: Include temp sensor and uncomment:
//	temp = temperature_sensor_get(void);

	*size = cc_coder_sizeof_float(temp);
	if (cc_platform_mem_alloc((void **)data, *size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	*data = cc_coder_encode_float(*data, temp);

	return CC_SUCCESS;
}

static cc_result_t cc_calvinsys_temp_sensor_close(struct cc_calvinsys_obj_t *obj)
{
	cc_platform_mem_free((void *)obj->state);
	return CC_SUCCESS;
}

static cc_calvinsys_obj_t *cc_calvinsys_temp_sensor_open(cc_calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char *capability_name)
{
	cc_calvinsys_obj_t *obj = NULL;

	if (cc_platform_mem_alloc((void **)&obj, sizeof(cc_calvinsys_obj_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	obj->can_write = NULL;
	obj->write = NULL;
	obj->can_read = cc_calvinsys_temp_sensor_can_read;
	obj->read = cc_calvinsys_temp_sensor_read;
	obj->close = cc_calvinsys_temp_sensor_close;
	obj->handler = handler;
	obj->next = NULL;
	obj->state = NULL;

	return obj;
}

cc_result_t cc_calvinsys_temp_sensor_create(cc_calvinsys_t **calvinsys, const char *name)
{
	cc_calvinsys_handler_t *handler = NULL;

	if (cc_platform_mem_alloc((void **)&handler, sizeof(cc_calvinsys_handler_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	handler->open = cc_calvinsys_temp_sensor_open;
	handler->objects = NULL;
	handler->next = NULL;

	cc_calvinsys_add_handler(calvinsys, handler);
	if (cc_calvinsys_register_capability(*calvinsys, name, handler, NULL) != CC_SUCCESS)
		return CC_FAIL;

	return CC_SUCCESS;
}
