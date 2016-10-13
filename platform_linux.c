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
#include "platform.h"

static calvin_gpio_t *m_gpios[MAX_GPIOS];

void platform_init(void)
{
	int i = 0;

	srand(time(NULL));

	for (i = 0; i < MAX_GPIOS; i++)
		m_gpios[i] = NULL;
}

void platform_run(void)
{
	// node_run is the main loop
}

calvin_gpio_t *create_in_gpio(uint32_t pin, char pull, char edge)
{
    int i = 0;

    if (pull != 'u' && pull != 'd') {
    	log_error("Unsupported pull direction");
    	return NULL;
    }

    if (edge != 'r' && edge != 'f' && edge != 'b') {
    	log_error("Unsupported edge");
    	return NULL;
    }

	for (i = 0; i < MAX_GPIOS; i++) {
		if (m_gpios[i] == NULL) {
			m_gpios[i] = (calvin_gpio_t *)malloc(sizeof(calvin_gpio_t));
			if (m_gpios[i] == NULL) {
				log_error("Failed to allocate memory");
				return NULL;
			}

			m_gpios[i]->pin = pin;
			m_gpios[i]->has_triggered = true;
			return m_gpios[i];
		}
	}

	return NULL;
}

calvin_gpio_t *create_out_gpio(uint32_t pin)
{
    int i = 0;

	for (i = 0; i < MAX_GPIOS; i++) {
		if (m_gpios[i] == NULL) {
			m_gpios[i] = (calvin_gpio_t *)malloc(sizeof(calvin_gpio_t));
			if (m_gpios[i] == NULL) {
				log_error("Failed to allocate memory");
				return NULL;
			}

			m_gpios[i]->pin = pin;
			return m_gpios[i];
		}
	}

	return NULL;
}

void uninit_gpio(calvin_gpio_t *gpio)
{
	int i = 0;

	for (i = 0; i < MAX_GPIOS; i++) {
		if (m_gpios[i] != NULL && m_gpios[i]->pin == gpio->pin) {
			m_gpios[i] = NULL;
			break;
		}		
	}

	free(gpio);
}

void set_gpio(calvin_gpio_t *gpio, uint32_t value)
{
	log("Setting gpio");
}

result_t get_temperature(double *temp)
{
	*temp = 15,5;
	return SUCCESS;
}
