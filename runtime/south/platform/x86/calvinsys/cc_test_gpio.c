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
#include "runtime/north/cc_node.h"
#include "runtime/north/coder/cc_coder.h"
#include "runtime/south/platform/cc_platform.h"
#include "calvinsys/cc_calvinsys.h"

typedef enum {
	CC_GPIO_IN,
	CC_GPIO_OUT
} cc_gpio_direction;

typedef struct cc_gpio_state_t {
	int pin;
	cc_gpio_direction direction;
	uint32_t value;
	uint32_t readings;
} cc_gpio_state_t;

static bool cc_gpio_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t cc_gpio_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	uint32_t value = 0;

	if (cc_coder_decode_uint(data, &value) == CC_SUCCESS) {
		cc_log("Platformx86: Setting gpio '%d'", (int)value);
		return CC_SUCCESS;
	}

	cc_log_error("Failed to decode data");
	return CC_FAIL;
}

static bool cc_gpio_can_read(struct cc_calvinsys_obj_t *obj)
{
	return ((cc_gpio_state_t *)obj->state)->readings < 5;
}

static cc_result_t cc_gpio_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	cc_gpio_state_t *state = (cc_gpio_state_t *)obj->state;
	char *buffer = NULL, *w = NULL;

	if (state->value == 1)
		state->value = 0;
	else
		state->value = 0;

	if (cc_platform_mem_alloc((void **)&buffer, cc_coder_sizeof_uint(state->value)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	w = buffer;
	w = cc_coder_encode_uint(buffer, state->value);
	*data = buffer;
	*size = w - buffer;
	state->readings++;

	return CC_SUCCESS;
}

static cc_result_t cc_gpio_close(struct cc_calvinsys_obj_t *obj)
{
	cc_platform_mem_free(obj->state);

	return CC_SUCCESS;
}

cc_result_t cc_test_gpio_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	char *init_args = (char *)obj->capability->init_args;
	char *direction = NULL;
	uint32_t pin = 0, len = 0;
	cc_gpio_state_t *state = NULL;

	if (cc_coder_decode_string_from_map(init_args, "direction", &direction, &len) != CC_SUCCESS) {
		cc_log_error("Failed to get 'direction'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(init_args, "pin", &pin) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'pin'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_gpio_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	if (strncmp(direction, "out", len) == 0) {
		cc_log("Platformx86: Opened pin '%d' as output", pin);
		state->direction = CC_GPIO_OUT;
	} else {
		cc_log("Platformx86: Opened pin '%d' as input", pin);
		state->direction = CC_GPIO_IN;
	}
	state->pin = pin;
	state->readings = 0;
	state->value = 0;

	obj->can_write = cc_gpio_can_write;
	obj->write = cc_gpio_write;
	obj->can_read = cc_gpio_can_read;
	obj->read = cc_gpio_read;
	obj->close = cc_gpio_close;
	obj->state = state;

	return CC_SUCCESS;
}
