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
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include "../../platform.h"
#include "../../transport_common.h"
#include "../../transport.h"
#include "../../node.h"

#define PLATFORM_RECEIVE_BUFFER_SIZE 512

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
			if (platform_mem_alloc((void **)&gpiohandler->ingpios[i], sizeof(calvin_ingpio_t)) != SUCCESS) {
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
	return SUCCESS;
}

static void platform_set_gpio(uint32_t pin, uint32_t value)
{
	log("Setting gpio pin '%d' '%d'", pin, value);
}

static void platform_uninit_gpio(calvinsys_io_giohandler_t *gpiohandler, uint32_t pin, gpio_direction_t direction)
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
	return SUCCESS;
}

static result_t platform_create_sensors_environmental(node_t *node)
{
	char name[] = "calvinsys.sensors.environmental";
	calvinsys_sensors_environmental_t *sensors_env = NULL;

	if (platform_mem_alloc((void **)&sensors_env, sizeof(calvinsys_sensors_environmental_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	sensors_env->get_temperature = platform_get_temperature;

 	return list_add_n(&node->calvinsys, name, strlen(name), sensors_env, sizeof(calvinsys_sensors_environmental_t));
}

static result_t platform_create_io_gpiohandler(node_t *node)
{
	char name[] = "calvinsys.io.gpiohandler";
	calvinsys_io_giohandler_t *io_gpiohandler = NULL;
	int i = 0;

	if (platform_mem_alloc((void **)&io_gpiohandler, sizeof(calvinsys_io_giohandler_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		platform_mem_free((void *)io_gpiohandler);
		return FAIL;
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
	if (platform_create_sensors_environmental(node) != SUCCESS)
		return FAIL;

	if (platform_create_io_gpiohandler(node) != SUCCESS)
		return FAIL;

	return SUCCESS;
}

void platform_init(void)
{
	srand(time(NULL));
}

static void platform_x86_handle_data(transport_client_t *transport_client)
{
	char buffer[PLATFORM_RECEIVE_BUFFER_SIZE];
	int size = 0;

	memset(&buffer, 0, PLATFORM_RECEIVE_BUFFER_SIZE);

	size = recv(transport_client->fd, buffer, PLATFORM_RECEIVE_BUFFER_SIZE, 0);
	if (size < 0)
		log_error("Failed to read data");
	else if (size == 0) {
		log("Disconnected");
		transport_client->state = TRANSPORT_DISCONNECTED;
	} else
		transport_handle_data(transport_client, buffer, size);
}

void platform_run(node_t *node, const char *iface, const int port)
{
	fd_set set;
	struct timeval reconnect_timeout = {10, 0};

	node->transport_client = transport_create();
	if (node->transport_client == NULL)
		return;

	while (true) {
		if (node->transport_client->state != TRANSPORT_CONNECTED) {
			if (transport_connect(node->transport_client, iface, port) != SUCCESS) {
				select(1, NULL, NULL, NULL, &reconnect_timeout);
				continue;
			} else
				node_transmit();
		}

		FD_ZERO(&set);
		FD_SET(node->transport_client->fd, &set);

		if (select(node->transport_client->fd + 1, &set, NULL, NULL, NULL) < 0)
			log_error("ERROR on select");
		else {
			platform_x86_handle_data(node->transport_client);
		}
	}
}

result_t platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		log_error("Failed to allocate '%ld' memory", (unsigned long)size);
		return FAIL;
	}

	log_debug("Allocated '%ld'", (unsigned long)size);

	return SUCCESS;
}

void platform_mem_free(void *buffer)
{
	free(buffer);
}
