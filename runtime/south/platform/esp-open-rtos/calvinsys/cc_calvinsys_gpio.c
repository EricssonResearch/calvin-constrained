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
#include <string.h>
#include <espressif/esp_common.h>
#include "esp8266.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/coder/cc_coder.h"
#include "runtime/south/platform/cc_platform.h"
#include "calvinsys/cc_calvinsys.h"

typedef enum {
	CC_GPIO_IN,
	CC_GPIO_OUT
} cc_calvinsys_gpio_direction;

typedef struct cc_calvinsys_gpio_state_t {
	uint8_t pin;
	cc_calvinsys_gpio_direction direction;
} cc_calvinsys_gpio_state_t;

static bool cc_calvinsys_gpio_can_write(struct cc_calvinsys_obj_t *obj)
{
	return ((cc_calvinsys_gpio_state_t *)obj->state)->direction == CC_GPIO_OUT;
}

static cc_result_t cc_calvinsys_gpio_write(struct cc_calvinsys_obj_t *obj, char *data, size_t data_size)
{
	uint32_t value = 0;

	if (cc_coder_decode_uint(data, &value) == CC_SUCCESS) {
		gpio_write(((cc_calvinsys_gpio_state_t *)obj->state)->pin, value);
		return CC_SUCCESS	;
	}

	return CC_FAIL;
}

static cc_result_t cc_calvinsys_gpio_close(struct cc_calvinsys_obj_t *obj)
{
	cc_calvinsys_gpio_state_t *gpio_state = (cc_calvinsys_gpio_state_t *)obj->state;

	gpio_disable(gpio_state->pin);
	cc_platform_mem_free(gpio_state);

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_gpio_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	char *init_args = (char *)obj->capability->init_args;
	char *direction = NULL;
	uint32_t pin = 0, len = 0;
	cc_calvinsys_gpio_state_t *state = NULL;

	if (cc_coder_decode_string_from_map(init_args, "direction", &direction, &len) != CC_SUCCESS) {
		cc_log_error("Failed to get 'direction'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(init_args, "pin", &pin) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'pin'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_calvinsys_gpio_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	if (strncmp(direction, "out", len) == 0) {
		state->direction = CC_GPIO_OUT;
		gpio_enable(pin, GPIO_OUTPUT);
		gpio_write(pin, 0);
	} else {
		cc_log_error("Unsupported direction");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	state->pin = pin;
	obj->can_write = cc_calvinsys_gpio_can_write;
	obj->write = cc_calvinsys_gpio_write;
	obj->close = cc_calvinsys_gpio_close;
	obj->state = state;

	return CC_SUCCESS;
}
