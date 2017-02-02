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
#include "../../platform.h"
#include "../../node.h"
#include "../../transport.h"

static calvin_gpio_t *m_gpios[MAX_GPIOS];

void platform_init(void)
{
	int i = 0;

	srand(time(NULL));

	for (i = 0; i < MAX_GPIOS; i++) {
		m_gpios[i] = NULL;
	}
}

result_t platform_evt_wait(void)
{
	return transport_select(60);
}

void platform_run(const char *ssdp_iface, const char *proxy_iface, const int proxy_port)
{
	uint32_t timeout = 60;

	if (node_start(ssdp_iface, proxy_iface, proxy_port) != SUCCESS) {
		log_error("Failed to start node");
		return;
	}

	while (1) {
		if (transport_select(timeout) != SUCCESS) {
			log_error("Failed to receive data");
			break;
		}
	}

	node_stop(true);
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
