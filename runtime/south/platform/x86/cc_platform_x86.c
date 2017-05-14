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
#include "../../../../msgpuck/msgpuck.h"
#include "../../../north/cc_msgpack_helper.h"


void platform_print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	fprintf(stdout, "\n");
	va_end(args);
}

result_t platform_stop(node_t *node)
{
  return CC_RESULT_SUCCESS;
}

result_t platform_node_started(struct node_t *node)
{
	return CC_RESULT_SUCCESS;
}

// calvinsys functions
static bool platform_temp_can_read(struct calvinsys_obj_t *obj)
{
	return true;
}

static result_t platform_temp_read(struct calvinsys_obj_t *obj, char **data, size_t *size)
{
	double temp = 15.5;

	*size = mp_sizeof_double(temp);
	if (platform_mem_alloc((void **)data, *size) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	mp_encode_double(*data, temp);

	return CC_RESULT_SUCCESS;
}

static calvinsys_obj_t *platform_temp_open(calvinsys_handler_t *handler, char *data, size_t len)
{
	calvinsys_obj_t *obj = NULL;

	if (platform_mem_alloc((void **)&obj, sizeof(calvinsys_obj_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	obj->write = NULL;
	obj->can_read = platform_temp_can_read;
	obj->read = platform_temp_read;
	obj->close = NULL;
	obj->handler = handler;
	obj->next = NULL;
	handler->objects = obj; // assume only one object

	return obj;
}

static result_t platform_create_calvinsys_temp(node_t *node)
{
	calvinsys_handler_t *handler = NULL;

	if (platform_mem_alloc((void **)&handler, sizeof(calvinsys_handler_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	handler->open = platform_temp_open;
	handler->objects = NULL;
	calvinsys_register_handler(&node->calvinsys, "calvinsys.sensors.environmental", handler);

	return CC_RESULT_SUCCESS;
}

static result_t platform_gpio_write(struct calvinsys_obj_t *obj, char *data, size_t size)
{
	uint32_t value = 0;

	if (decode_uint(data, &value) == CC_RESULT_SUCCESS) {
		log("Setting GPIO with '%d'", (int)value);
		return CC_RESULT_SUCCESS;
	}

	log_error("Failed to decode GPIO data");
	return CC_RESULT_FAIL;
}

static bool platform_gpio_can_read(struct calvinsys_obj_t *obj)
{
	return true;
}

static result_t platform_gpio_read(struct calvinsys_obj_t *obj, char **data, size_t *size)
{
	static uint32_t value = 1;

	*size = mp_sizeof_uint(value);
	if (platform_mem_alloc((void **)data, *size) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	mp_encode_uint(*data, value);

	if (value == 1)
		value = 0;
	else
		value = 1;

	return CC_RESULT_SUCCESS;
}

static result_t platform_gpio_close(struct calvinsys_obj_t *obj)
{
	log("Closing gpio");
	return CC_RESULT_SUCCESS;
}

static calvinsys_obj_t *platform_gpio_open(calvinsys_handler_t *handler, char *data, size_t len)
{
	uint32_t pin = 0, value_len = 0;
	char *direction = NULL, *edge = NULL, *pull = NULL;
	calvinsys_obj_t *obj = NULL;

	if (platform_mem_alloc((void **)&obj, sizeof(calvinsys_obj_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	if (decode_uint_from_map(data, "pin", &pin) != CC_RESULT_SUCCESS) {
		log_error("Failed to decode 'pin'");
		return NULL;
	}

	if (decode_string_from_map(data, "direction", &direction, &value_len) != CC_RESULT_SUCCESS) {
		log_error("Failed to decode 'direction'");
		return NULL;
	}

	if (direction[0] == 'i') {
		if (decode_string_from_map(data, "edge", &edge, &value_len) != CC_RESULT_SUCCESS) {
			log_error("Failed to decode 'edge'");
			return NULL;
		}

		if (decode_string_from_map(data, "pull", &pull, &value_len) != CC_RESULT_SUCCESS) {
			log_error("Failed to decode 'pull'");
			return NULL;
		}
		obj->write = NULL;
		obj->can_read = platform_gpio_can_read;
		obj->read = platform_gpio_read;
		obj->close = platform_gpio_close;
		log("Opened GPIO pin '%d' as input with direction '%c' edge '%c' pull '%c'", (int)pin, *direction, *edge, *pull);
	} else {
		obj->write = platform_gpio_write;
		obj->can_read = NULL;
		obj->read = NULL;
		obj->close = platform_gpio_close;
		log("Opened GPIO pin '%d' as output", (int)pin);
	}

	obj->write = platform_gpio_write;
	obj->can_read = platform_gpio_can_read;
	obj->read = platform_gpio_read;
	obj->close = NULL;
	obj->handler = handler;
	obj->next = NULL;
	handler->objects = obj; // assume only one object

	return obj;
}

static result_t platform_create_calvinsys_gpio(node_t *node)
{
	calvinsys_handler_t *handler = NULL;

	if (platform_mem_alloc((void **)&handler, sizeof(calvinsys_handler_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	handler->open = platform_gpio_open;
	handler->objects = NULL;
	calvinsys_register_handler(&node->calvinsys, "calvinsys.io.gpiohandler", handler);

	return CC_RESULT_SUCCESS;
}

// end of calvinsys functions

result_t platform_create_calvinsys(node_t *node)
{
	if (platform_create_calvinsys_temp(node) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (platform_create_calvinsys_gpio(node) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	return CC_RESULT_SUCCESS;
}

void platform_init(void)
{
	srand(time(NULL));
}

result_t platform_create(node_t *node)
{
  node_attributes_t *attr;

	if (platform_mem_alloc((void **)&attr, sizeof(node_attributes_t)) != CC_RESULT_SUCCESS)
    log_error("Could not allocate memory for attributes");

	attr->indexed_public_owner = NULL;
  attr->indexed_public_node_name = NULL;
  attr->indexed_public_address = NULL;
  node->attributes = attr;

	return CC_RESULT_SUCCESS;
}

bool platform_evt_wait(node_t *node, uint32_t timeout_seconds)
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

	if (node->transport_client != NULL && (node->transport_client->state == TRANSPORT_PENDING || node->transport_client->state == TRANSPORT_ENABLED)) {
		FD_SET(((transport_socket_client_t *)node->transport_client->client_state)->fd, &fds);
		fd = ((transport_socket_client_t *)node->transport_client->client_state)->fd;

		select(fd + 1, &fds, NULL, NULL, tv_ref);

		if (FD_ISSET(fd, &fds)) {
			if (transport_handle_data(node, node->transport_client, node_handle_message) != CC_RESULT_SUCCESS) {
				log_error("Failed to read data from transport");
				node->transport_client->state = TRANSPORT_DISCONNECTED;
			}
			return true;
		}
	} else
		select(0, NULL, NULL, NULL, tv_ref);

	return false;
}

result_t platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		log_error("Failed to allocate '%ld' memory", (unsigned long)size);
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

void *platform_mem_calloc(size_t nitems, size_t size)
{
  void *ptr = NULL;
  if (platform_mem_alloc(&ptr, nitems * size) != CC_RESULT_SUCCESS)
    return NULL;

  memset(ptr, 0, nitems * size);
  return ptr;
}

void platform_mem_free(void *buffer)
{
	free(buffer);
}

#ifdef CC_DEEPSLEEP_ENABLED
void platform_deepsleep(node_t *node)
{
	log("Going to deepsleep state, runtime will stop!");
}
#endif

#ifdef CC_STORAGE_ENABLED
void platform_write_node_state(node_t *node, char *buffer, size_t size)
{
	FILE *fp = NULL;
	int len = 0;

	fp = fopen("calvinconstrained.config", "w+");
	if (fp != NULL) {
		len = fwrite(buffer, 1, size, fp);
		if (len != size)
			log_error("Failed to write node config");
		else
			log_debug("Wrote runtime state '%d' bytes", len);
		fclose(fp);
	} else
		log("Failed to open calvinconstrained.config for writing");
}

result_t platform_read_node_state(node_t *node, char buffer[], size_t size)
{
	FILE *fp = NULL;

	fp = fopen("calvinconstrained.config", "r+");
	if (fp != NULL) {
		fread(buffer, 1, size, fp);
		fclose(fp);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}
#endif
