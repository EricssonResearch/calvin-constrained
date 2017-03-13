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
#include "../../platform.h"
#include "platform.h"
#include "calvinsys_android.h"
#include "../../node.h"
#include "../../api.h"
#include <unistd.h>
#include <stdarg.h>
#include <android/log.h>
#include <android/sensor.h>
#include "platform_android.h"

static result_t send_upstream_platform_message(const node_t* node, char* cmd, transport_client_t* tc, size_t data_size)
{
	// TODO: Avoid double buffering
	transport_append_buffer_prefix(tc->tx_buffer.buffer, data_size+3);
	char buffer[BUFFER_SIZE];

	memset(&buffer, 0, BUFFER_SIZE);
	memcpy(buffer, tc->tx_buffer.buffer, 4); // Copy total size to output
	memcpy(buffer+4, cmd, 2); // Copy 2 byte command
	memcpy(buffer+6, tc->tx_buffer.buffer+4, data_size); // Copy payload data
	// Write
	if (write(((android_platform_t*) node->platform)->upstream_platform_fd[1], buffer, data_size+6) < 0)
		log_error("Could not write to pipe");
	transport_free_tx_buffer(tc);
	return SUCCESS;
}

static result_t send_downstream_platform_message(const node_t* node, char* cmd, transport_client_t* tc, char* data, size_t data_size)
{
	// TODO: Avoid double buffering
	// Add command in the first 2 bytes
	char buffer[BUFFER_SIZE];

	memset(buffer, 0, BUFFER_SIZE);
	memcpy(buffer, cmd, 2);
	memcpy(buffer+2, data, data_size);
	if (write(((android_platform_t*) node->platform)->downstream_platform_fd[1], buffer, data_size+2) < 0)
		log_error("Could not write to pipe");
	return SUCCESS;
}

static result_t read_upstream(const node_t* node, char* buffer, size_t size)
{
	int fd = ((android_platform_t*) node->platform)->upstream_platform_fd[0];
	fd_set set;

	FD_ZERO(&set);
	FD_SET(fd, &set);
	int status = select(fd+1, &set, NULL, NULL, NULL);

	if (status > 0) {
		int bytes_read = read(fd, buffer, size);

		if (bytes_read < 0) {
			log_error("Error when reading from pipe");
			return SUCCESS;
		} else {
			return SUCCESS;
		}
	} else if (status < 0) {
		log_error("Error on upstream select");
		return FAIL;
	}
	return SUCCESS;
}

void platform_print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	__android_log_vprint(ANDROID_LOG_VERBOSE, "Calvin Constrained Android", fmt, args);
	va_end(args);
}

static result_t command_calvin_msg(node_t* node, char* payload_data, size_t size)
{
	transport_client_t* tc = node->transport_client;

	transport_handle_data(node, tc, payload_data, size);
	return SUCCESS;
}

static result_t command_rt_stop(node_t* node, char* payload_data, size_t size)
{
	node->state = NODE_STOP;
	return SUCCESS;
}

static result_t platform_transport_connected(node_t* node, char* data, size_t size)
{
	transport_join(node, node->transport_client);
	return SUCCESS;
}

struct platform_command_handler_t platform_command_handlers[NBR_OF_COMMANDS] = {
		{CONNECT_REPLY, platform_transport_connected},
		{RUNTIME_STOP, command_rt_stop},
		{RUNTIME_CALVIN_MSG, command_calvin_msg}
};

result_t platform_create(node_t* node)
{
	android_platform_t* platform;

	if (platform_mem_alloc((void**)&platform, sizeof(android_platform_t)) != SUCCESS) {
		log_error("Could not allocate memory for platform object.");
		return FAIL;
	}
	platform->send_downstream_platform_message = send_downstream_platform_message;
	platform->send_upstream_platform_message = send_upstream_platform_message;
	platform->read_upstream = read_upstream;

	node->platform = platform;
	if (pipe(((android_platform_t*) node->platform)->upstream_platform_fd) < 0 || pipe(((android_platform_t*) node->platform)->downstream_platform_fd) < 0) {
		log_error("Could not open pipes for transport");
		return FAIL;
	}
	return SUCCESS;
}

result_t handle_platform_call(node_t* node, int fd)
{
	char data_buffer[BUFFER_SIZE];
	int readstatus = read(fd, data_buffer, BUFFER_SIZE);

	if (readstatus < 0) {
		log_error("Could not read data size");
		return FAIL;
	}
	size_t size = get_message_len(data_buffer+2);

	char cmd[3];
	memset(cmd, 0, 3);

	memcpy(cmd, data_buffer, 2);
	if (size == 0)
		log("No payload data for command");

	// Handle command
	int i;
	for (i = 0; i < NBR_OF_COMMANDS; i++) {
		if (strcmp(platform_command_handlers[i].command, cmd) == 0) {
			log("will handle that command");
			platform_command_handlers[i].handler(node, data_buffer+2, size+4);
			return SUCCESS;
		}
	}
	log_error("Command %s not found", cmd);
	return FAIL;
}

static result_t platform_android_handle_data(node_t* node, transport_client_t *transport_client)
{
	log("platform handle data");
	result_t result = handle_platform_call(node, ((android_platform_t*) node->platform)->downstream_platform_fd[0]);
	if (result != SUCCESS)
		log_error("fcm_handle platform call failed");
	return result;
}

result_t platform_create_calvinsys(struct node_t *node)
{   android_platform_t* platform;

	platform = (android_platform_t*) node->platform;
	platform->looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
	log("Create calvinsys");

	create_calvinsys(node);
	return SUCCESS;
}

void platform_init(void)
{
	srand(time(NULL));
}

static int transport_fd_handler(int fd, int events, void *data)
{
	node_t* node = (node_t*) data;
	log("Transport fd triggered");
	if (platform_android_handle_data(node, node->transport_client) != SUCCESS) {
		log_error("Error when handling data");
	}
}

void platform_evt_wait(node_t *node, struct timeval *timeout)
{
	android_platform_t* platform;

	int timeout_trigger = 5000;
	int transport_trigger_id = 2;

	platform = (android_platform_t*) node->platform;
	if (node->transport_client == NULL) {
		log_error("tp was null.");
		return;
	}
	if (ALooper_addFd(platform->looper, ((android_platform_t*) node->platform)->downstream_platform_fd[0], transport_trigger_id, ALOOPER_EVENT_INPUT, &transport_fd_handler, node) != 1) {
		log_error("Could not add fd to looper, looper: %p", platform->looper);
		sleep(5);
	}

	int status = ALooper_pollOnce(timeout_trigger, NULL, NULL, NULL);
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
