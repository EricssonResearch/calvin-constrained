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

#define PLATFORM_RECEIVE_BUFFER_SIZE 512

static calvin_gpio_t *m_gpios[MAX_GPIOS];

void platform_init()
{
	int i = 0;

	srand(time(NULL));

	for (i = 0; i < MAX_GPIOS; i++) {
		m_gpios[i] = NULL;
	}
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

calvin_gpio_t *platform_create_in_gpio(uint32_t pin, char pull, char edge)
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

	for (i = 0; i < MAX_GPIOS; i++) {
		if (m_gpios[i] == NULL) {
			if (platform_mem_alloc((void **)&m_gpios[i], sizeof(calvin_gpio_t)) != SUCCESS) {
				log_error("Failed to allocate memory");
				return NULL;
			}

			m_gpios[i]->pin = pin;
			m_gpios[i]->has_triggered = false;
			return m_gpios[i];
		}
	}

	return NULL;
}

calvin_gpio_t *platform_create_out_gpio(uint32_t pin)
{
	int i = 0;

	for (i = 0; i < MAX_GPIOS; i++) {
		if (m_gpios[i] == NULL) {
			if (platform_mem_alloc((void **)&m_gpios[i], sizeof(calvin_gpio_t)) != SUCCESS) {
				log_error("Failed to allocate memory");
				return NULL;
			}

			m_gpios[i]->pin = pin;
			return m_gpios[i];
		}
	}

	return NULL;
}

void platform_uninit_gpio(calvin_gpio_t *gpio)
{
	int i = 0;

	for (i = 0; i < MAX_GPIOS; i++) {
		if (m_gpios[i] != NULL && m_gpios[i]->pin == gpio->pin) {
			m_gpios[i] = NULL;
			break;
		}
	}

	log("Freeing gpio '%d'", gpio->pin);
	platform_mem_free((void *)gpio);
}

void platform_set_gpio(calvin_gpio_t *gpio, uint32_t value)
{
	log("Setting gpio pin '%d' '%d'", gpio->pin, value);
	gpio->value = value;
}

result_t platform_get_temperature(double *temp)
{
	*temp = 15.5;
	return SUCCESS;
}
