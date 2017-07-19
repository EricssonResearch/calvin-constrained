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
#include <espressif/esp_common.h>
#include "cc_calvinsys_yl69.h"
#include "../../../../north/cc_node.h"
#include "../../cc_platform.h"
#include "../../../../../calvinsys/cc_calvinsys.h"
#include "../../../../../msgpuck/msgpuck.h"

static bool calvinsys_yl69_can_read(struct calvinsys_obj_t *obj)
{
	return true;
}

static result_t calvinsys_yl69_read(struct calvinsys_obj_t *obj, char **data, size_t *size)
{
  uint16_t adc_value = sdk_system_adc_read();
	float humidity = 1024 - adc_value;

	humidity = humidity / 1024;
	humidity = humidity * 100;

	*size = mp_sizeof_float(humidity);
	if (platform_mem_alloc((void **)data, *size) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}
	mp_encode_float(*data, humidity);

	return CC_RESULT_SUCCESS;
}

static result_t calvinsys_yl69_close(struct calvinsys_obj_t *obj)
{
	return CC_RESULT_SUCCESS;
}

static calvinsys_obj_t *calvinsys_yl69_open(calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id)
{
	calvinsys_obj_t *obj = NULL;

	if (platform_mem_alloc((void **)&obj, sizeof(calvinsys_obj_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	obj->can_write = NULL;
	obj->write = NULL;
	obj->can_read = calvinsys_yl69_can_read;
	obj->read = calvinsys_yl69_read;
	obj->close = calvinsys_yl69_close;
	obj->handler = handler;
	obj->next = NULL;
	obj->state = NULL;
	handler->objects = obj; // assume only one object

	return obj;
}

result_t calvinsys_yl69_create(calvinsys_t **calvinsys, const char *name)
{
	calvinsys_handler_t *handler = NULL;

	if (platform_mem_alloc((void **)&handler, sizeof(calvinsys_handler_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	handler->open = calvinsys_yl69_open;
	handler->objects = NULL;
	handler->next = NULL;

	calvinsys_add_handler(calvinsys, handler);
	if (calvinsys_register_capability(*calvinsys, name, handler, NULL) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	return CC_RESULT_SUCCESS;
}
