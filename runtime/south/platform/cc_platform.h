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
#include "../../north/cc_common.h"
#include "../../../calvinsys/cc_calvinsys.h"

struct node_t;
struct transport_client_t;

/**
 * platform_init() - Initialize the platform.
 *
 * Called before the node is created to initialize the platform.
 */
void platform_init(void);

/**
 * platform_print() - Print a printf string.
 * @fmt Printf format string
 * ... Additional arguments replacing format specifiers in fmt.
 */
void platform_print(const char *fmt, ...);

/**
 * platform_create() - Create a platform object.
 * @node the node object
 *
 * Called when the node has been created to create a platform object
 * on node->platform.
 *
 * Return: CC_RESULT_SUCCESS/CC_RESULT_FAILURE
 */
result_t platform_create(struct node_t *node);

/**
 * platform_create_calvinsys() - Create calvinsys objects.
 * @node the node object
 *
 * Called when node is starting to create calvinsys objects, the
 * calvinsys objects should be added to node->calvinsys.
 *
 * Return: CC_RESULT_SUCCESS/CC_RESULT_FAILURE
 */
result_t platform_create_calvinsys(calvinsys_t **calvinsys);

/**
 * platform_mem_alloc() - Allocate requested memory.
 * @buffer buffer to allocate
 * @size size of the memory block to allocate
 *
 * Return: CC_RESULT_SUCCESS/CC_RESULT_FAILURE
 */
result_t platform_mem_alloc(void **buffer, uint32_t size);

/**
 * platform_mem_calloc() - Allocate requested memory.
 * @nitems The number of elements to be allocated
 * @size The size he size of elements
 *
 * Return: Pointer to the allocated memory or NULL if failure
 */
void *platform_mem_calloc(size_t nitems, size_t size);

/**
 * platform_mem_free() - Deallocates memory previously allocated by a call to platform_mem_alloc/platoform_mem_calloc.
 * @buffer buffer to deallocate
 */
void platform_mem_free(void *buffer);

/**
 * platform_evt_wait() - Wait for an event
 * @node the node
 * @timeout_seconds time in seconds to block waiting for an event
 *
 * The call should block waiting for:
 * - Data is available on a transport interface, received data from a transport interface is
 *   handled by calling transport_handle_data defined in transport.h.
 * - A event from a platform function has triggered (such as a timer expiring or a digitial input toggling).
 * - The timeout expires.
 *
 * Return: true if triggered, false if timeout triggered
 */
bool platform_evt_wait(struct node_t *node, uint32_t timeout_seconds);

/**
 * platform_stop() - Called when the platform stops
 * @node the node
 *
 * Return: CC_RESULT_SUCCESS/CC_RESULT_FAILURE
 */
result_t platform_stop(struct node_t *node);

/**
 * platform_node_started() - Called when the node has started
 * @node the node
 *
 * Return: CC_RESULT_SUCCESS/CC_RESULT_FAILURE
 */
result_t platform_node_started(struct node_t *node);

/**
 * platform_node_get_seconds() - Get seconds since start
 *
 * Return: Seconds since start
 */
uint32_t platform_get_seconds(struct node_t *node);

#ifdef CC_DEEPSLEEP_ENABLED
/**
 * platform_deepsleep() - Enter platform deep sleep state.
 * @node the node
 * @time time to sleep
 */
void platform_deepsleep(struct node_t *node, uint32_t time);
#endif

#ifdef CC_STORAGE_ENABLED
/**
 * platform_write_node_state() - Write serialized node state to nonvolatile memory.
 * @buffer the serialized data to write
 * @size the size of the serialized dat
 */
void platform_write_node_state(struct node_t *node, char *buffer, size_t size);

/**
 * platform_read_node_state() - Read serialized node state from persistent media.
 * @buffer the read serialized data
 * @size the size of the serialized data
 *
 * Return: SUCCESS/FAILURE
 */
result_t platform_read_node_state(struct node_t *node, char buffer[], size_t size);
#endif

#ifdef MBEDTLS_NO_PLATFORM_ENTROPY
/**
 * platform_random_vector_generate() - Random number generator
 * @ctx Context registered with the library on creation of the TLS instance
 * @buffer Buffer where generated random vector is to be fetched
 * @size Requested size of the random vector
 *
 * Return: 0 if success
 */
int platform_random_vector_generate(void *ctx, unsigned char *buffer, size_t size);
#endif

#ifdef CC_DEBUG
#define cc_log_debug(a, args...) platform_print("DEBUG: %s(%s:%d) "a"",  __func__, __FILE__, __LINE__, ##args)
#else
#define cc_log_debug(a, args...) do {} while (0)
#endif
#define cc_log_error(a, args...) platform_print("ERROR: %s(%s:%d) "a"",  __func__, __FILE__, __LINE__, ##args)
#define cc_log(a, args...) platform_print(a, ##args)

#endif /* PLATFORM_H */
