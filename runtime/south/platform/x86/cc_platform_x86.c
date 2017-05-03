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

static calvin_ingpio_t *platform_init_in_gpio(calvinsys_io_giohandler_t *gpiohandler, uint32_t pin, char pull, char edge)
{
	int i = 0;

	if (pull != 'u' && pull != 'd') {
		log_error("Unsupported pull direction '%c'", pull);
		return NULL;
	}

	if (edge != 'r' && edge != 'f' && edge != 'b') {
		log_error("Unsupported edge '%c'", edge);
		return NULL;
	}

	for (i = 0; i < MAX_INGPIOS; i++) {
		if (gpiohandler->ingpios[i] == NULL) {
			if (platform_mem_alloc((void **)&gpiohandler->ingpios[i], sizeof(calvin_ingpio_t)) != CC_RESULT_SUCCESS) {
				log_error("Failed to allocate memory");
				return NULL;
			}

			gpiohandler->ingpios[i]->pin = pin;
			gpiohandler->ingpios[i]->has_triggered = false;
			gpiohandler->ingpios[i]->pull = pull;
			gpiohandler->ingpios[i]->edge = edge;
			return gpiohandler->ingpios[i];
		}
	}

	return NULL;
}

static result_t platform_init_out_gpio(uint32_t pin)
{
	return CC_RESULT_SUCCESS;
}

static void platform_set_gpio(uint32_t pin, uint32_t value)
{
	log("Setting gpio pin '%d' '%d'", pin, value);
}

static void platform_uninit_gpio(calvinsys_io_giohandler_t *gpiohandler, uint32_t pin, calvin_gpio_direction_t direction)
{
	int i = 0;

	for (i = 0; i < MAX_INGPIOS; i++) {
		if (gpiohandler->ingpios[i] != NULL && gpiohandler->ingpios[i]->pin == pin) {
			platform_mem_free((void *)gpiohandler->ingpios[i]);
			gpiohandler->ingpios[i] = NULL;
			log("Released in gpio '%d'", pin);
			return;
		}
	}
}

static result_t platform_get_temperature(double *temp)
{
	*temp = 15.5;
	return CC_RESULT_SUCCESS;
}

static result_t platform_create_sensors_environmental(node_t *node)
{
	char name[] = "calvinsys.sensors.environmental";
	calvinsys_sensors_environmental_t *sensors_env = NULL;

	if (platform_mem_alloc((void **)&sensors_env, sizeof(calvinsys_sensors_environmental_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	sensors_env->get_temperature = platform_get_temperature;

	return list_add_n(&node->calvinsys, name, strlen(name), sensors_env, sizeof(calvinsys_sensors_environmental_t));
}

static result_t platform_create_io_gpiohandler(node_t *node)
{
	char name[] = "calvinsys.io.gpiohandler";
	calvinsys_io_giohandler_t *io_gpiohandler = NULL;
	int i = 0;

	if (platform_mem_alloc((void **)&io_gpiohandler, sizeof(calvinsys_io_giohandler_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		platform_mem_free((void *)io_gpiohandler);
		return CC_RESULT_FAIL;
	}

	io_gpiohandler->init_in_gpio = platform_init_in_gpio;
	io_gpiohandler->init_out_gpio = platform_init_out_gpio;
	io_gpiohandler->set_gpio = platform_set_gpio;
	io_gpiohandler->uninit_gpio = platform_uninit_gpio;

	for (i = 0; i < MAX_INGPIOS; i++)
		io_gpiohandler->ingpios[i] = NULL;

	return list_add_n(&node->calvinsys, name, strlen(name), io_gpiohandler, sizeof(calvinsys_io_giohandler_t));
}

result_t platform_create_calvinsys(node_t *node)
{
	if (platform_create_sensors_environmental(node) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (platform_create_io_gpiohandler(node) != CC_RESULT_SUCCESS)
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

void platform_evt_wait(node_t *node, struct timeval *timeout)
{
	fd_set fds;
	int fd = 0;

	FD_ZERO(&fds);

	if (node->transport_client != NULL && (node->transport_client->state == TRANSPORT_PENDING || node->transport_client->state == TRANSPORT_ENABLED)) {
		FD_SET(((transport_socket_client_t *)node->transport_client->client_state)->fd, &fds);
		fd = ((transport_socket_client_t *)node->transport_client->client_state)->fd;

		select(fd + 1, &fds, NULL, NULL, timeout);

		if (FD_ISSET(fd, &fds)) {
			if (transport_handle_data(node, node->transport_client, node_handle_message) != CC_RESULT_SUCCESS) {
				log_error("Failed to read data from transport");
				node->transport_client->state = TRANSPORT_DISCONNECTED;
				return;
			}
		}
	} else
		select(0, NULL, NULL, NULL, timeout);
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

#ifdef USE_PERSISTENT_STORAGE
void platform_write_node_state(node_t *node, char *buffer, size_t size)
{
	FILE *fp = NULL;

	fp = fopen("calvinconstrained.config", "w+");
	if (fp != NULL) {
		if (fwrite(buffer, 1, size, fp) != size)
			log_error("Failed to write node config");
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
