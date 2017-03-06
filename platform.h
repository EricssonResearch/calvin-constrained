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
#include <sys/time.h>
#include "common.h"
#ifdef NRF52
#include "app_trace.h"
#else
#include <stdio.h>
#endif
#ifdef PLATFORM_ANDROID
#include <android/log.h>
#endif
#define MAX_INGPIOS 5

struct node_t;
struct transport_client_t;

void platform_print(const char *fmt, ...);

#ifdef DEBUG
#define log_debug(a, args...) platform_print("DEBUG: %s(%s:%d) "a"",  __func__, __FILE__, __LINE__, ##args)
#else
#define log_debug(a, args...) do {} while (0)
#endif
#define log_error(a, args...) platform_print("ERROR: %s(%s:%d) "a"",  __func__, __FILE__, __LINE__, ##args)
#define log(a, args...) platform_print(a, ##args)

typedef enum {
	GPIO_IN,
	GPIO_OUT
} gpio_direction_t;

typedef struct calvin_ingpio_t {
	uint32_t pin;
	bool has_triggered;
	uint32_t value;
	char pull;
	char edge;
} calvin_ingpio_t;

typedef struct calvinsys_io_giohandler_t {
	calvin_ingpio_t *(*init_in_gpio)(struct calvinsys_io_giohandler_t *gpiohandler, uint32_t pin, char pull, char edge);
	result_t (*init_out_gpio)(uint32_t pin);
  void (*set_gpio)(uint32_t pin, uint32_t value);
	void (*uninit_gpio)(struct calvinsys_io_giohandler_t *gpiohandler, uint32_t pin, gpio_direction_t direction);
	calvin_ingpio_t *ingpios[MAX_INGPIOS];
} calvinsys_io_giohandler_t;

typedef struct calvinsys_sensors_environmental_t {
  result_t (*get_temperature)(double *temp);
} calvinsys_sensors_environmental_t;

typedef struct platform_t {
#ifdef PLATFORM_ANDROID
	int upstream_platform_fd[2]; // read end [0], write end [1]
	int downstream_platform_fd[2];
#endif
} platform_t;

#ifdef PLATFORM_ANDROID
struct platform_command_handler_t {
	char command[50];
	result_t (*handler)(struct node_t* node, char *data, size_t size);
};
result_t platform_android_handle_data(struct node_t* node, struct transport_client_t *transport_client);
#endif

void platform_init(struct node_t *node, char* name);
result_t platform_create_calvinsys(struct node_t *node);
void platform_evt_wait(struct node_t *node, struct timeval *timeout);
result_t platform_mem_alloc(void **buffer, uint32_t size);
void platform_mem_free(void *buffer);
result_t platform_create(struct node_t* node);

#ifdef USE_PERSISTENT_STORAGE
void platform_write_node_state(char *buffer, size_t size);
result_t platform_read_node_state(char buffer[], size_t size);
#endif

#endif /* PLATFORM_H */
