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
#include "esp8266.h"
#include "cc_calvinsys_gpio.h"
#include "../../../../north/cc_node.h"
#include "../../../../north/coder/cc_coder.h"
#include "../../cc_platform.h"
#include "../../../../../calvinsys/cc_calvinsys.h"

static bool cc_calvinsys_gpio_can_write(struct cc_calvinsys_obj_t *obj)
{
	return ((cc_calvinsys_gpio_state_t *)obj->state)->direction == CC_GPIO_OUT;
}

static cc_result_t cc_calvinsys_gpio_write(struct cc_calvinsys_obj_t *obj, char *data, size_t data_size)
{
	uint32_t value = 0;

	if (cc_coder_decode_uint(data, &value) == CC_SUCCESS) {
		gpio_write(((cc_calvinsys_gpio_state_t *)obj->state)->pin, value);
		return CC_SUCCESS;
	}

	return CC_FAIL;
}

static bool cc_calvinsys_gpio_can_read(struct cc_calvinsys_obj_t *obj)
{
	return false;
}

static cc_result_t cc_calvinsys_gpio_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	return CC_FAIL;
}

static cc_result_t cc_calvinsys_gpio_close(struct cc_calvinsys_obj_t *obj)
{
	return CC_SUCCESS;
}

static cc_calvinsys_obj_t *cc_calvinsys_gpio_open(cc_calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char *capability_name)
{
	cc_calvinsys_obj_t *obj = NULL;
	cc_calvinsys_gpio_state_t *gpio_state = (cc_calvinsys_gpio_state_t *)state;

	if (gpio_state->direction == CC_GPIO_OUT)
		gpio_enable(gpio_state->pin, GPIO_OUTPUT);
	else {
		cc_log_error("Unsupported direction");
		return NULL;
	}

	if (cc_platform_mem_alloc((void **)&obj, sizeof(cc_calvinsys_obj_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	obj->can_write = cc_calvinsys_gpio_can_write;
	obj->write = cc_calvinsys_gpio_write;
	obj->can_read = cc_calvinsys_gpio_can_read;
	obj->read = cc_calvinsys_gpio_read;
	obj->close = cc_calvinsys_gpio_close;
	obj->handler = handler;
	obj->next = NULL;
	obj->state = state;
	handler->objects = obj; // assume only one object

	return obj;
}

cc_calvinsys_handler_t *cc_calvinsys_gpio_create_handler(cc_calvinsys_t **calvinsys)
{
	cc_calvinsys_handler_t *handler = NULL;

	if (cc_platform_mem_alloc((void **)&handler, sizeof(cc_calvinsys_handler_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	handler->open = cc_calvinsys_gpio_open;
	handler->objects = NULL;
	handler->next = NULL;

	cc_calvinsys_add_handler(calvinsys, handler);

	return handler;
}
