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
#include "../../../../north/cc_msgpack_helper.h"
#include "../../cc_platform.h"
#include "../../../../../calvinsys/cc_calvinsys.h"
#include "../../../../../msgpuck/msgpuck.h"

static bool calvinsys_gpio_can_write(struct calvinsys_obj_t *obj)
{
	return ((calvinsys_gpio_state_t *)obj->state)->direction == CC_GPIO_OUT;
}

static result_t calvinsys_gpio_write(struct calvinsys_obj_t *obj, char *data, size_t data_size)
{
	uint32_t value = 0;

	if (decode_uint(data, &value) == CC_RESULT_SUCCESS) {
		gpio_write(((calvinsys_gpio_state_t *)obj->state)->pin, value);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

static bool calvinsys_gpio_can_read(struct calvinsys_obj_t *obj)
{
	return false;
}

static result_t calvinsys_gpio_read(struct calvinsys_obj_t *obj, char **data, size_t *size)
{
	return CC_RESULT_FAIL;
}

static result_t calvinsys_gpio_close(struct calvinsys_obj_t *obj)
{
	return CC_RESULT_SUCCESS;
}

static calvinsys_obj_t *calvinsys_gpio_open(calvinsys_handler_t *handler, char *data, size_t len, void *state)
{
	calvinsys_obj_t *obj = NULL;
	calvinsys_gpio_state_t *gpio_state = (calvinsys_gpio_state_t *)state;

	if(gpio_state->direction == CC_GPIO_OUT)
		gpio_enable(gpio_state->pin, GPIO_OUTPUT);
	else {
		cc_log_error("Unsupported direction");
		return NULL;
	}

	if (platform_mem_alloc((void **)&obj, sizeof(calvinsys_obj_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	obj->can_write = calvinsys_gpio_can_write;
	obj->write = calvinsys_gpio_write;
	obj->can_read = calvinsys_gpio_can_read;
	obj->read = calvinsys_gpio_read;
	obj->close = calvinsys_gpio_close;
	obj->handler = handler;
	obj->next = NULL;
	obj->state = state;
	handler->objects = obj; // assume only one object

	return obj;
}

calvinsys_handler_t *calvinsys_gpio_create_handler(calvinsys_t **calvinsys)
{
	calvinsys_handler_t *handler = NULL;

	if (platform_mem_alloc((void **)&handler, sizeof(calvinsys_handler_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	handler->open = calvinsys_gpio_open;
	handler->objects = NULL;
	handler->next = NULL;

	calvinsys_add_handler(calvinsys, handler);

	return handler;
}
