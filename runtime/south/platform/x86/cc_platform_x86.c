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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cc_config.h"
#include "runtime/south/platform/cc_platform.h"
#include "runtime/south/transport/socket/cc_transport_socket.h"
#include "runtime/north/cc_transport.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/cc_common.h"
#include "calvinsys/cc_calvinsys.h"
#include "runtime/north/coder/cc_coder.h"

// calvinsys types

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

typedef struct cc_temperature_state_t {
	bool can_read;
	float value;
} cc_temperature_state_t;

// end of calvinsys types

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
static bool cc_platformx86_temp_can_read(struct cc_calvinsys_obj_t *obj)
{
	return ((cc_temperature_state_t *)obj->state)->can_read;
}

static cc_result_t cc_platformx86_temp_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	cc_temperature_state_t *state = (cc_temperature_state_t *)obj->state;

	if (!state->can_read)
		return CC_FAIL;

	*size = cc_coder_sizeof_double(state->value);
	if (cc_platform_mem_alloc((void **)data, *size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	cc_coder_encode_double(*data, state->value);
	state->value++;
	state->can_read = false;

	return CC_SUCCESS;
}

static bool cc_platformx86_temp_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t cc_platformx86_temp_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	cc_temperature_state_t *state = (cc_temperature_state_t *)obj->state;

	if (cc_coder_decode_bool(data, &state->can_read) != CC_SUCCESS) {
		cc_log_error("Failed to decode value");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static cc_result_t cc_platformx86_temp_close(struct cc_calvinsys_obj_t *obj)
{
	cc_platform_mem_free(obj->state);

	return CC_SUCCESS;
}

cc_result_t cc_platformx86_temp_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	cc_temperature_state_t *state = NULL;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_temperature_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	state->can_read = false;
	state->value = 15.5;

	obj->can_write = cc_platformx86_temp_can_write;
	obj->write = cc_platformx86_temp_write;
	obj->can_read = cc_platformx86_temp_can_read;
	obj->read = cc_platformx86_temp_read;
	obj->close = cc_platformx86_temp_close;
	obj->state = state;

	return CC_SUCCESS;
}

static bool cc_platformx86_gpio_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t cc_platformx86_gpio_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	uint32_t value = 0;

	if (cc_coder_decode_uint(data, &value) == CC_SUCCESS) {
		cc_log("Platformx86: Setting gpio '%d'", (int)value);
		return CC_SUCCESS;
	}

	cc_log_error("Failed to decode data");
	return CC_FAIL;
}

static bool cc_platformx86_gpio_can_read(struct cc_calvinsys_obj_t *obj)
{
	return ((cc_gpio_state_t *)obj->state)->readings < 5;
}

static cc_result_t cc_platformx86_gpio_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
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

static cc_result_t cc_platformx86_gpio_close(struct cc_calvinsys_obj_t *obj)
{
	cc_platform_mem_free(obj->state);

	return CC_SUCCESS;
}

cc_result_t cc_platformx86_gpio_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
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

	obj->can_write = cc_platformx86_gpio_can_write;
	obj->write = cc_platformx86_gpio_write;
	obj->can_read = cc_platformx86_gpio_can_read;
	obj->read = cc_platformx86_gpio_read;
	obj->close = cc_platformx86_gpio_close;
	obj->state = state;

	return CC_SUCCESS;
}

// end of calvinsys functions

void cc_platform_init(void)
{
	srand(time(NULL));
}

cc_result_t cc_platform_create(cc_node_t *node)
{
	return CC_SUCCESS;
}

cc_platform_evt_wait_status_t cc_platform_evt_wait(cc_node_t *node, uint32_t timeout_seconds)
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
			if (cc_transport_handle_data(node, node->transport_client, cc_node_handle_message) != CC_SUCCESS) {
				cc_log_error("Failed to handle received data");
				return CC_PLATFORM_EVT_WAIT_FAIL;
			}
			return CC_PLATFORM_EVT_WAIT_DATA_READ;
		}
	} else
		select(0, NULL, NULL, NULL, tv_ref);

	return CC_PLATFORM_EVT_WAIT_TIMEOUT;
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

uint32_t cc_platform_get_time(void)
{
	struct timeval value;

	gettimeofday(&value, NULL);

	return value.tv_sec;
}

#if CC_USE_SLEEP
void cc_platform_deepsleep(uint32_t time_in_us)
{
	exit(0);
}
#endif

#if CC_USE_STORAGE
cc_stat_t cc_platform_file_stat(const char *path)
{
	struct stat statbuf;

	if (stat(path, &statbuf) == 0) {
		if (S_ISDIR(statbuf.st_mode))
			return CC_STAT_DIR;
		if (S_ISREG(statbuf.st_mode))
			return CC_STAT_FILE;
	}

	return CC_STAT_NO_EXIST;
}

cc_result_t cc_platform_file_read(const char *path, char **buffer, size_t *len)
{
	FILE *fp = NULL;
	size_t read = 0;

	fp = fopen(path, "r+");
	if (fp == NULL) {
		cc_log_error("Failed to open '%s'", path);
		return CC_FAIL;
	}

	fseek(fp, 0, SEEK_END);
	*len = ftell(fp);

	if (cc_platform_mem_alloc((void **)buffer, *len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	memset(*buffer, 0, *len);

	rewind(fp);
	read = fread(*buffer, 1, *len, fp);
	fclose(fp);

	if (read != *len) {
		cc_platform_mem_free(*buffer);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static cc_result_t cc_platform_create_dirs(const char *path)
{
	int i = 0, len = strlen(path);
	char *tmp = NULL;

	while (i < len) {
		if (path[i] == '/') {
			if (cc_platform_mem_alloc((void **)&tmp, i + 1) != CC_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				return CC_FAIL;
			}
			strncpy(tmp, path, i);
			tmp[i] = '\0';
			if (cc_platform_file_stat(tmp) == CC_STAT_NO_EXIST) {
				if (mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
					cc_log_error("Failed to create '%s'", tmp);
					cc_platform_mem_free(tmp);
					return CC_FAIL;
				}
			}
			cc_platform_mem_free(tmp);
		}
		i++;
	}
	return CC_SUCCESS;
}

cc_result_t cc_platform_file_write(const char *path, char *buffer, size_t size)
{
	FILE *fp = NULL;
	int len = 0;

	if (cc_platform_create_dirs(path) != CC_SUCCESS)
		return CC_FAIL;

	fp = fopen(path, "w+");
	if (fp == NULL)
		return CC_FAIL;

	len = fwrite(buffer, 1, size, fp);
	if (len != size) {
		cc_log_error("Failed to write node config");
		fclose(fp);
		return CC_FAIL;
	}

	fclose(fp);

	return CC_SUCCESS;
}
#endif
