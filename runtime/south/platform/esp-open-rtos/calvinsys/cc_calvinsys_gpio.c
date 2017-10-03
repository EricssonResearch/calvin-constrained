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
	return ((cc_calvinsys_gpio_state_t *)obj->capability->state)->direction == CC_GPIO_OUT;
}

static cc_result_t cc_calvinsys_gpio_write(struct cc_calvinsys_obj_t *obj, char *data, size_t data_size)
{
	uint32_t value = 0;

	if (cc_coder_decode_uint(data, &value) == CC_SUCCESS) {
		gpio_write(((cc_calvinsys_gpio_state_t *)obj->capability->state)->pin, value);
		return CC_SUCCESS;
	}

	return CC_FAIL;
}

static cc_result_t cc_calvinsys_gpio_open(cc_calvinsys_obj_t *obj, char *data, size_t len)
{
	cc_calvinsys_gpio_state_t *gpio_state = (cc_calvinsys_gpio_state_t *)obj->capability->state;

	if (gpio_state->direction == CC_GPIO_OUT)
		gpio_enable(gpio_state->pin, GPIO_OUTPUT);
	else {
		cc_log_error("Unsupported direction");
		return CC_FAIL;
	}

	obj->can_write = cc_calvinsys_gpio_can_write;
	obj->write = cc_calvinsys_gpio_write;

	return CC_SUCCESS;
}

static cc_result_t cc_calvinsys_gpio_deserialize(cc_calvinsys_obj_t *obj, char *buffer)
{
	return cc_calvinsys_gpio_open(obj, buffer, 0);
}

cc_result_t cc_calvinsys_gpio_create(cc_calvinsys_t **calvinsys, const char *name, cc_calvinsys_gpio_state_t *state)
{
	return cc_calvinsys_create_capability(*calvinsys,
		name,
		cc_calvinsys_gpio_open,
		cc_calvinsys_gpio_deserialize,
		NULL);
}
