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
#include "common.h"

#ifdef NRF51
#include "app_trace.h"
#else
#include <stdio.h>
#include <time.h>
#endif

#ifdef NRF51
#ifdef DEBUG
#define log_debug(a, args...) app_trace_log("DEBUG: %s(%s:%d) "a"\r\n",  __func__,__FILE__, __LINE__, ##args)
#else
#define log_debug(fmt, ...) do {} while (0)
#endif
#define log_error(a, args...) app_trace_log("ERROR: %s(%s:%d) "a"\r\n",  __func__,__FILE__, __LINE__, ##args)
#define log_dump app_trace_dump
#define log(a, args...) app_trace_log(a"\r\n", ##args)
#else
#ifdef DEBUG
#define log_debug(a, args...) printf("DEBUG: %s(%s:%d) "a"\n",  __func__,__FILE__, __LINE__, ##args)
#else
#define log_debug(fmt, ...) do {} while (0)
#endif
#define log_error(a, args...) printf("ERROR: %s(%s:%d) "a"\n",  __func__,__FILE__, __LINE__, ##args)
#define log(a, args...) printf(a"\r\n", ##args)
#endif

typedef struct calvin_timer_t {
	double interval;
#ifdef NRF51
	uint32_t last_triggered;
#else
	time_t last_triggered;
#endif
} calvin_timer_t;

void platform_init();
calvin_timer_t *create_recurring_timer(double interval);
void stop_timer(calvin_timer_t *timer);
bool check_timer(calvin_timer_t *timer);

#endif /* PLATFORM_H */