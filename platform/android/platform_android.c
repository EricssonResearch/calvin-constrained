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
#include "../../node.h"
#include "../../api.h"
#include <unistd.h>
#include <stdarg.h>
#include <android/log.h>

#define PLATFORM_RECEIVE_BUFFER_SIZE 512

#define NBR_OF_COMMANDS 3

void platform_print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	__android_log_vprint(ANDROID_LOG_VERBOSE, "Calvin Constrained Android", fmt, args);
	va_end(args);
}

static result_t command_calvin_msg(node_t* node, char* payload_data, size_t size) {
	transport_client_t* tc = node->transport_client;
	transport_handle_data(node, tc, payload_data, size);
	return SUCCESS;
}

static result_t command_rt_stop(node_t* node, char* payload_data, size_t size) {
	node->state = NODE_STOP;
	return SUCCESS;
}

static result_t platform_transport_connected(node_t* node)
{
	transport_join(node, node->transport_client);
}

struct platform_command_handler_t platform_command_handlers[NBR_OF_COMMANDS] = {
		{CONNECT_REPLY, platform_transport_connected},
		{RUNTIME_STOP, command_rt_stop},
		{RUNTIME_CALVIN_MSG, command_calvin_msg}
};

result_t platform_create_calvinsys(node_t *node)
{
	// TODO: Create sys obj
	return SUCCESS;
}

result_t platform_create(node_t* node)
{
	platform_t* platform;
	if (platform_mem_alloc(&platform, sizeof(platform_t)) != SUCCESS) {
		log_error("Could not allocate memory for platform object.");
		return FAIL;
	}
	node->platform = platform;

#ifdef PLATFORM_PIPE
	if (pipe(node->platform->upstream_platform_fd) < 0 || pipe(node->platform->downstream_platform_fd) < 0){
		log_error("Could not open pipes for transport");
		return FAIL;
	}
	log_debug("UPSTREAM PIPE: %d (read), %d (write)", node->platform->upstream_platform_fd[0], node->platform->upstream_platform_fd[1]);
	log_debug("DOWNSTREAM PIPE: %d (read), %d (write)", node->platform->upstream_platform_fd[0], node->platform->upstream_platform_fd[1]);
#endif

	log_debug("Platform created");
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

	if (size == 0) {
		log("No payload data for command");
	}

	// Handle command
	int i;
	for (i=0; i < NBR_OF_COMMANDS; i++) {
		if (strcmp(platform_command_handlers[i].command, cmd) == 0) {
			platform_command_handlers[i].handler(node, data_buffer+2, size+4);
			return SUCCESS;
		}
	}
	log_error("Command %s not found", cmd);
	return FAIL;
}

result_t platform_android_handle_data(node_t* node, transport_client_t *transport_client)
{
	result_t result = handle_platform_call(node, transport_client->downstream_fd[0]);
	if (result != SUCCESS) {
		log_error("fcm_handle platform call failed");
	}
	return result;
}

void platform_init(node_t* node, char* name)
{
	if (platform_create_calvinsys(node) != SUCCESS) {
		log_error("Failed to create calvinsys");
		return;
	}

	if (node_create(node, name) != SUCCESS) {
		log_error("Failed to create node");
		return;
	}
	// If this is FCM, the transport must be created here instead
	srand(time(NULL));
}

void platform_evt_wait(node_t *node, struct timeval *timeout)
{
	fd_set set;
	struct timeval reconnect_timeout = {10, 0};

	if (node->transport_client == NULL) {
		log_error("tp was null.");
		return;
	}

	FD_ZERO(&set);

	FD_SET(node->transport_client->downstream_fd[0], &set);
	int max_fd = 1;
	int status = select(node->transport_client->downstream_fd[0] + max_fd, &set, NULL, NULL, NULL);
	if (status > 0) {
		if (platform_android_handle_data(node, node->transport_client) != SUCCESS) {
			log_error("Error when handling data");
		}
	}
	else if (status == 0) {
		log_debug("Select timeout.");
	} else {
		log_error("ERROR on select");
	}
	//sleep(3);
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