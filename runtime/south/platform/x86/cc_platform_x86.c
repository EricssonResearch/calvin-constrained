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
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include "../cc_platform.h"
#include "../../transport/socket/cc_transport_socket.h"
#include "../../../north/cc_transport.h"
#include "../../../north/cc_node.h"
#include "../../../north/cc_common.h"
#include "../../../../calvinsys/cc_calvinsys.h"
#include "../../../north/coder/cc_coder.h"

typedef enum {
	CC_GPIO_IN,
	CC_GPIO_OUT
} cc_calvinsys_gpio_direction;

typedef struct cc_calvinsys_gpio_state_t {
	int pin;
	cc_calvinsys_gpio_direction direction;
} cc_calvinsys_gpio_state_t;

void cc_platform_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	fprintf(stdout, "\n");
	va_end(args);
}

cc_result_t cc_platform_stop(cc_node_t *node)
{
	return CC_SUCCESS;
}

cc_result_t cc_platform_node_started(struct cc_node_t *node)
{
	return CC_SUCCESS;
}

// calvinsys functions
static bool platform_temp_can_read(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t platform_temp_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	double temp = 15.5;

	*size = cc_coder_sizeof_double(temp);
	if (cc_platform_mem_alloc((void **)data, *size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	cc_coder_encode_double(*data, temp);

	return CC_SUCCESS;
}

static bool platform_calvinsys_temp_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t platform_calvinsys_temp_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	return CC_SUCCESS;
}

static cc_calvinsys_obj_t *platform_temp_open(cc_calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char *capability_name)
{
	cc_calvinsys_obj_t *obj = NULL;

	if (cc_platform_mem_alloc((void **)&obj, sizeof(cc_calvinsys_obj_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	obj->can_write = platform_calvinsys_temp_can_write;
	obj->write = platform_calvinsys_temp_write;
	obj->can_read = platform_temp_can_read;
	obj->read = platform_temp_read;
	obj->close = NULL;
	obj->handler = handler;
	obj->next = NULL;
	obj->state = NULL;

	return obj;
}

static cc_calvinsys_handler_t *cc_platform_create_temperature_handler(void)
{
	cc_calvinsys_handler_t *handler = NULL;

	if (cc_platform_mem_alloc((void **)&handler, sizeof(cc_calvinsys_handler_t)) == CC_SUCCESS) {
		handler->open = platform_temp_open;
		handler->objects = NULL;
		handler->next = NULL;
	} else
		cc_log_error("Failed to allocate memory");

	return handler;
}

static bool platform_calvinsys_digitial_in_out_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t platform_calvinsys_digitial_in_out_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	uint32_t value = 0;

	if (cc_coder_decode_uint(data, &value) == CC_SUCCESS) {
		cc_log("Setting digitial out with '%d'", (int)value);
		return CC_SUCCESS;
	}

	cc_log_error("Failed to decode data");
	return CC_FAIL;
}

static bool platform_calvinsys_digitial_in_out_can_read(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t platform_calvinsys_digitial_in_out_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	static uint32_t value = 1;

	*size = cc_coder_sizeof_uint(value);
	if (cc_platform_mem_alloc((void **)data, *size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	cc_coder_encode_uint(*data, value);

	if (value == 1)
		value = 0;
	else
		value = 1;

	return CC_SUCCESS;
}

static cc_result_t platform_calvinsys_digitial_in_out_close(struct cc_calvinsys_obj_t *obj)
{
	cc_log("Closing digitial in/out");
	return CC_SUCCESS;
}

static cc_calvinsys_obj_t *platform_calvinsys_digitial_in_out_open(cc_calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char *capability_name)
{
	cc_calvinsys_obj_t *obj = NULL;
	cc_calvinsys_gpio_state_t *gpio_state = (cc_calvinsys_gpio_state_t *)state;

	if (gpio_state->direction == CC_GPIO_IN)
		cc_log("Opening pin '%d' as input", gpio_state->pin);
	else if (gpio_state->direction == CC_GPIO_OUT)
		cc_log("Opening pin '%d' as output", gpio_state->pin);
	else {
		cc_log_error("Unknown direction");
		return NULL;
	}

	if (cc_platform_mem_alloc((void **)&obj, sizeof(cc_calvinsys_obj_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	obj->can_write = platform_calvinsys_digitial_in_out_can_write;
	obj->write = platform_calvinsys_digitial_in_out_write;
	obj->can_read = platform_calvinsys_digitial_in_out_can_read;
	obj->read = platform_calvinsys_digitial_in_out_read;
	obj->close = platform_calvinsys_digitial_in_out_close;
	obj->handler = handler;
	obj->state = NULL;
	obj->next = NULL;

	return obj;
}

static cc_calvinsys_handler_t *cc_platform_create_digitial_in_out_handler(void)
{
	cc_calvinsys_handler_t *handler = NULL;

	if (cc_platform_mem_alloc((void **)&handler, sizeof(cc_calvinsys_handler_t)) == CC_SUCCESS) {
		handler->open = platform_calvinsys_digitial_in_out_open;
		handler->objects = NULL;
		handler->next = NULL;
	} else
		cc_log_error("Failed to allocate memory");

	return handler;
}

// end of calvinsys functions

cc_result_t cc_platform_create_calvinsys(cc_calvinsys_t **calvinsys)
{
	cc_calvinsys_handler_t *handler = NULL;
	cc_calvinsys_gpio_state_t *state_light = NULL, *state_button = NULL;

	handler = cc_platform_create_temperature_handler();
	if (handler == NULL)
		return CC_FAIL;

	cc_calvinsys_add_handler(calvinsys, handler);
	if (cc_calvinsys_register_capability(*calvinsys, "io.temperature", handler, NULL) != CC_SUCCESS)
		return CC_FAIL;

	handler = cc_platform_create_digitial_in_out_handler();
	if (handler == NULL)
		return CC_FAIL;

	cc_calvinsys_add_handler(calvinsys, handler);

	if (cc_platform_mem_alloc((void **)&state_light, sizeof(cc_calvinsys_gpio_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	state_light->pin = 1;
	state_light->direction = CC_GPIO_OUT;

	if (cc_calvinsys_register_capability(*calvinsys, "io.light", handler, state_light) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_platform_mem_alloc((void **)&state_button, sizeof(cc_calvinsys_gpio_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	state_button->pin = 2;
	state_button->direction = CC_GPIO_IN;

	if (cc_calvinsys_register_capability(*calvinsys, "io.button", handler, state_button) != CC_SUCCESS)
		return CC_FAIL;

	return CC_SUCCESS;
}

void cc_platform_init(void)
{
	srand(time(NULL));
}

cc_result_t cc_platform_create(cc_node_t *node)
{
	return CC_SUCCESS;
}

bool cc_platform_evt_wait(cc_node_t *node, uint32_t timeout_seconds)
{
	fd_set fds;
	int fd = 0;
	struct timeval tv, *tv_ref = NULL;

	if (timeout_seconds > 0) {
		tv.tv_sec = timeout_seconds;
		tv.tv_usec = 0;
		tv_ref = &tv;
	}

	FD_ZERO(&fds);

	if (node->transport_client != NULL && (node->transport_client->state == CC_TRANSPORT_PENDING || node->transport_client->state == CC_TRANSPORT_ENABLED)) {
		FD_SET(((cc_transport_socket_client_t *)node->transport_client->client_state)->fd, &fds);
		fd = ((cc_transport_socket_client_t *)node->transport_client->client_state)->fd;

		select(fd + 1, &fds, NULL, NULL, tv_ref);

		if (FD_ISSET(fd, &fds)) {
			if (cc_transport_handle_data(node, node->transport_client, cc_node_handle_message) != CC_SUCCESS)
				cc_log_error("Failed to handle received data");
			return true;
		}
	} else
		select(0, NULL, NULL, NULL, tv_ref);

	return false;
}

cc_result_t cc_platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		cc_log_error("Failed to allocate '%ld' memory", (unsigned long)size);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

void *cc_platform_mem_calloc(size_t nitems, size_t size)
{
	void *ptr = NULL;

	if (cc_platform_mem_alloc(&ptr, nitems * size) != CC_SUCCESS)
		return NULL;

	memset(ptr, 0, nitems * size);
	return ptr;
}

void cc_platform_mem_free(void *buffer)
{
	free(buffer);
}

uint32_t cc_platform_get_seconds(cc_node_t *node)
{
	struct timeval value;

	gettimeofday(&value, NULL);

	return value.tv_sec;
}

#ifdef CC_DEEPSLEEP_ENABLED
void cc_platform_deepsleep(cc_node_t *node, uint32_t time)
{
	cc_log("Going to deepsleep state, runtime will stop!");
}
#endif

#ifdef CC_STORAGE_ENABLED
void cc_platform_write_node_state(cc_node_t *node, char *buffer, size_t size)
{
	FILE *fp = NULL;
	int len = 0;

	fp = fopen(CC_CONFIG_FILE, "w+");
	if (fp != NULL) {
		len = fwrite(buffer, 1, size, fp);
		if (len != size)
			cc_log_error("Failed to write node config");
		else
			cc_log_debug("Wrote runtime state '%d' bytes", len);
		fclose(fp);
	} else
		cc_log("Failed to open %s for writing", CC_CONFIG_FILE);
}

cc_result_t cc_platform_read_node_state(cc_node_t *node, char buffer[], size_t size)
{
	FILE *fp = NULL;

	fp = fopen(CC_CONFIG_FILE, "r+");
	if (fp != NULL) {
		fread(buffer, 1, size, fp);
		fclose(fp);
		return CC_SUCCESS;
	}

	return CC_FAIL;
}
#endif
