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
#ifndef CC_PLATFORM_H
#define CC_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include "cc_config.h"
#include "runtime/north/cc_common.h"
#include "calvinsys/cc_calvinsys.h"

struct cc_node_t;
struct cc_transport_client_t;

typedef enum {
	CC_PLATFORM_EVT_WAIT_DATA_READ,
	CC_PLATFORM_EVT_WAIT_FAIL,
  CC_PLATFORM_EVT_WAIT_TIMEOUT
} cc_platform_evt_wait_status_t;

typedef enum {
  CC_STAT_NO_EXIST,
  CC_STAT_DIR,
  CC_STAT_FILE
} cc_stat_t;

/**
 * cc_platform_init() - Initialize the platform.
 *
 * Called before the node is created to initialize the platform.
 */
void cc_platform_init(void);

/**
 * cc_platform_print() - Print a printf string.
 * @fmt Printf format string
 * ... Additional arguments replacing format specifiers in fmt.
 */
void cc_platform_print(const char *fmt, ...);

/**
 * cc_platform_create() - Create a platform object.
 * @node the node object
 *
 * Called when the node has been created to create a platform object
 * on node->platform.
 *
 * Return: CC_SUCCESS/CC_FAILURE
 */
cc_result_t cc_platform_create(struct cc_node_t *node);

/**
 * cc_platform_mem_alloc() - Allocate requested memory.
 * @buffer buffer to allocate
 * @size size of the memory block to allocate
 *
 * Return: CC_SUCCESS/CC_FAILURE
 */
cc_result_t cc_platform_mem_alloc(void **buffer, uint32_t size);

/**
 * cc_platform_mem_calloc() - Allocate requested memory.
 * @nitems The number of elements to be allocated
 * @size The size he size of elements
 *
 * Return: Pointer to the allocated memory or NULL if failure
 */
void *cc_platform_mem_calloc(size_t nitems, size_t size);

/**
 * cc_platform_mem_free() - Deallocates memory previously allocated by a call to cc_platform_mem_alloc/platoform_mem_calloc.
 * @buffer buffer to deallocate
 */
void cc_platform_mem_free(void *buffer);

/**
 * cc_platform_evt_wait() - Wait for an event
 * @node the node
 * @timeout_seconds time in seconds to block waiting for an event
 *
 * The call should block waiting for:
 * - Data is available on a transport interface, received data handled by calling transport_handle_data defined in transport.h.
 * - A event from a platform function has triggered (such as a timer expiring or a digitial input toggling).
 * - The timeout expires.
 *
 * Return: CC_PLATFORM_EVT_WAIT_DATA_READ if data was read, CC_PLATFORM_EVT_WAIT_FAIL on error, CC_PLATFORM_EVT_WAIT_TIMEOUT on timeout
 */
cc_platform_evt_wait_status_t cc_platform_evt_wait(struct cc_node_t *node, uint32_t timeout_seconds);

/**
 * cc_platform_stop() - Called when the platform stops
 * @node the node
 *
 * Return: CC_SUCCESS/CC_FAILURE
 */
cc_result_t cc_platform_stop(struct cc_node_t *node);

/**
 * cc_platform_node_started() - Called when the node has started
 * @node the node
 *
 * Return: CC_SUCCESS/CC_FAILURE
 */
cc_result_t cc_platform_node_started(struct cc_node_t *node);

/**
 * cc_platform_get_time() - Get system time (s)
 *
 * Return: System time in seconds since boot/reset
 */
uint32_t cc_platform_get_time(void);

#if CC_USE_SLEEP
/**
 * cc_platform_deepsleep() - Enter platform deep sleep state.
 * @time_in_us microseconds to sleep
 */
void cc_platform_deepsleep(uint32_t time_in_us);
#endif

#if CC_USE_STORAGE
/**
 * cc_platform_file_stat() - Get information about the file pointed by path
 * @path the path
 *
 * Return: CC_STAT_NO_EXIST, CC_STAT_FILE or CC_STAT_DIR
 */
cc_stat_t cc_platform_file_stat(const char *path);

/**
 * cc_platform_file_read() - Allocate and read file content to buffer
 * @path the path
 * @buffer the buffer to allocate and read to
 * @len bytes read
 *
 * Return: CC_SUCCESS if success or CC_FAIL on failure
 */
cc_result_t cc_platform_file_read(const char *path, char **buffer, size_t *len);

/**
 * cc_platform_file_write() - Write to file pointed by path
 * @path the path
 * @buffer buffer to write
 * @size size of the buffer
 *
 * Return: CC_SUCCESS if success or CC_FAIL on failure
 */
cc_result_t cc_platform_file_write(const char *path, char *buffer, size_t size);
#endif

#ifdef MBEDTLS_NO_PLATFORM_ENTROPY
/**
 * cc_platform_random_vector_generate() - Random number generator
 * @ctx Context registered with the library on creation of the TLS instance
 * @buffer Buffer where generated random vector is to be fetched
 * @size Requested size of the random vector
 *
 * Return: 0 if success
 */
int cc_platform_random_vector_generate(void *ctx, unsigned char *buffer, size_t size);
#endif

#if CC_DEBUG
#define cc_log_debug(a, args...) cc_platform_print("DEBUG: (%s:%d) "a"",  __func__, __LINE__, ##args)
#else
#define cc_log_debug(a, args...) do {} while (0)
#endif
#define cc_log_error(a, args...) cc_platform_print("ERROR: (%s:%d) "a"",  __func__, __LINE__, ##args)
#define cc_log(a, args...) cc_platform_print(a, ##args)

#endif /* CC_PLATFORM_H */
