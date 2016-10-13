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

#ifdef NRF51
#include "app_trace.h"
#else
#include <stdio.h>
#endif

#define MAX_GPIOS 5

#ifdef NRF51
#ifdef DEBUG
#define log_debug(a, args...) app_trace_log("DEBUG: %s(%s:%d) "a"\r\n",  __func__, __FILE__, __LINE__, ##args)
#else
#define log_debug(fmt, ...) do {} while (0)
#endif
#define log_error(a, args...) app_trace_log("ERROR: %s(%s:%d) "a"\r\n",  __func__, __FILE__, __LINE__, ##args)
#define log_dump app_trace_dump
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

typedef struct calvin_gpio_t {
	uint32_t pin;
	bool has_triggered;
    uint32_t value;
} calvin_gpio_t;

void platform_init(void);
void platform_run(void);
calvin_gpio_t *create_in_gpio(uint32_t pin, char pull, char edge);
calvin_gpio_t *create_out_gpio(uint32_t pin);
void set_gpio(calvin_gpio_t *gpio, uint32_t value);
void uninit_gpio(calvin_gpio_t *gpio);
result_t get_temperature(double *temp);

#endif /* PLATFORM_H */
