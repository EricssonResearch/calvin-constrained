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
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include "common.h"
#ifdef NRF52
#include "app_trace.h"
#else
#include <stdio.h>
#endif

#define MAX_GPIOS 5

#ifdef NRF52
#ifdef DEBUG
#define log_debug(a, args...) app_trace_log("DEBUG:" a "\r\n", ##args)
#else
#define log_debug(fmt, ...) do {} while (0)
#endif
#define log_error(a, args...) app_trace_log("ERROR: " a "\r\n", ##args)
#define log(a, args...) app_trace_log(a"\r\n", ##args)
#else
#ifdef DEBUG
#define log_debug(a, args...) printf("DEBUG: %s(%s:%d) "a"\n",  __func__, __FILE__, __LINE__, ##args)
#else
#define log_debug(fmt, ...) do {} while (0)
#endif
#define log_error(a, args...) printf("ERROR: %s(%s:%d) "a"\n",  __func__, __FILE__, __LINE__, ##args)
#define log(a, args...) printf(a"\r\n", ##args)
#endif

typedef enum {
	GPIO_IN,
	GPIO_OUT
} gpio_direction_t;

typedef enum {
	EDGE_RISING,
	EDGE_FALLING,
	EDGE_BOTH
} gpio_edge_t;

typedef enum {
	PULL_UP,
	PULL_DOWN
} gpio_pull_t;

typedef struct calvin_gpio_t {
	uint32_t pin;
	bool has_triggered;
	uint32_t value;
	gpio_direction_t direction;
	gpio_edge_t gpio_edge;
	gpio_pull_t gpio_pull;
} calvin_gpio_t;

void platform_init(void);
#ifdef LWM2M_HTTP_CLIENT
void platform_init_lwm2m(char *iface, int port, char *url);
#endif
result_t platform_run(const char *ssdp_iface, const char *proxy_iface, const int proxy_port);
result_t platform_mem_alloc(void **buffer, uint32_t size);
void platform_mem_free(void *buffer);
calvin_gpio_t *platform_create_in_gpio(uint32_t pin, char pull, char edge);
calvin_gpio_t *platform_create_out_gpio(uint32_t pin);
void platform_set_gpio(calvin_gpio_t *gpio, uint32_t value);
void platform_uninit_gpio(calvin_gpio_t *gpio);
result_t platform_get_temperature(double *temp);

#endif /* PLATFORM_H */
