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
#include "calvinsys_android.h"
#include <unistd.h>
#include <stdarg.h>
#include <android/log.h>
#include "platform_android.h"
#include <api.h>
#include <sys/socket.h>
#include <transport/socket/transport_socket.h>
#include <stdio.h>

#ifdef SLEEP_AT_UNACTIVITY
#include "time.h"
time_t last_activity;
#endif

#define PLATFORM_RECEIVE_BUFFER_SIZE 512

static result_t send_upstream_platform_message(const node_t* node, char* cmd, transport_client_t* tc, size_t data_size)
{
	// TODO: Avoid double buffering
	transport_append_buffer_prefix(tc->tx_buffer.buffer, data_size+2);
	char buffer[BUFFER_SIZE];
	log("write upstream message with payload size: %zu", data_size);
	memset(&buffer, 0, BUFFER_SIZE);
	memcpy(buffer, tc->tx_buffer.buffer, 4); // Copy total size to output
	memcpy(buffer+4, cmd, 2); // Copy 2 byte command
	memcpy(buffer+6, tc->tx_buffer.buffer+4, data_size); // Copy payload data
	// Write
	if (write(((android_platform_t*) node->platform)->upstream_platform_fd[1], buffer, data_size+6) < 0)
		log_error("Could not write to pipe");
	log("wrote upstream message of size: %d",  get_message_len(buffer));
	transport_free_tx_buffer(tc);
	log("Done writing");
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
		int bytes_read = read(fd, buffer, 4);
		unsigned int datasize = get_message_len(buffer);
		log("Read upstream message of size: %d", datasize);
		if (datasize > size-4) {
			log_error("Error when reading upstream data. Buffer is to small.");
		}
		bytes_read = read(fd, buffer+4, datasize);
		log("Bytes read: %d", bytes_read);
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
	return api_runtime_stop(node);
}

static result_t platform_transport_connected(node_t* node, char* data, size_t size)
{
	transport_join(node, node->transport_client);
	return SUCCESS;
}

static result_t platform_serialize_and_stop(node_t* node, char* data, size_t size)
{
	return api_runtime_serialize_and_stop(node);
}

static result_t platform_trigger_reconnect(node_t* node, char* data, size_t size)
{
	return api_reconnect(node);
}

struct platform_command_handler_t platform_command_handlers[NBR_OF_COMMANDS] = {
		{CONNECT_REPLY, platform_transport_connected},
		{RUNTIME_STOP, command_rt_stop},
		{RUNTIME_CALVIN_MSG, command_calvin_msg},
		{RUNTIME_SERIALIZE_AND_STOP, platform_serialize_and_stop},
		{RUNTIME_TRIGGER_RECONNECT, platform_trigger_reconnect}
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

	log("platform fd up; %d %d, down: %d %d", ((android_platform_t*) node->platform)->upstream_platform_fd[0], ((android_platform_t*) node->platform)->upstream_platform_fd[1], ((android_platform_t*) node->platform)->downstream_platform_fd[0], ((android_platform_t*) node->platform)->downstream_platform_fd[1]);
    
    node_attributes_t* attr;
    if (platform_mem_alloc((void **) &attr, sizeof(node_attributes_t)) != SUCCESS) {
        log_error("Could not allocate memory for attributes");
    }
    attr->indexed_public_owner = NULL;
    attr->indexed_public_node_name = NULL;
    attr->indexed_public_address = NULL;
    node->attributes = attr;
	
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

	// Handle command
	int i;
	for (i = 0; i < NBR_OF_COMMANDS; i++) {
		if (strcmp(platform_command_handlers[i].command, cmd) == 0) {
			log_debug("will handle command %s", cmd);
			platform_command_handlers[i].handler(node, data_buffer+2, size+4);
			return SUCCESS;
		}
	}
	log_error("Command %s not found", cmd);
	return FAIL;
}

static result_t platform_android_handle_data(node_t* node, transport_client_t *transport_client)
{
	result_t result = handle_platform_call(node, ((android_platform_t*) node->platform)->downstream_platform_fd[0]);
	if (result != SUCCESS)
		log_error("fcm_handle platform call failed");
	return result;
}

static result_t platform_android_handle_socket_data(node_t *node, transport_client_t *transport_client)
{
	char buffer[PLATFORM_RECEIVE_BUFFER_SIZE];
	int size = 0;

	size = recv(((transport_socket_client_t *)transport_client->client_state)->fd, buffer, PLATFORM_RECEIVE_BUFFER_SIZE, 0);
	if (size == 0)
		transport_client->state = TRANSPORT_DISCONNECTED;
	else if (size > 0)
		transport_handle_data(node, transport_client, buffer, size);
	else
		log_error("Failed to read data");
	return SUCCESS;
}

result_t platform_node_started(struct node_t* node)
{
	// Send RT started, tc is not created here, so just write on the pipe
	char buffer[6];
	memset(buffer, 0, 6);
	memset(buffer+3, 2 & 0xFF, 1);
	memcpy(buffer+4, RUNTIME_STARTED, 2);
	if (write(((android_platform_t*) node->platform)->upstream_platform_fd[1], buffer, 6) < 0) {
		log_error("Failed to write rt started command");
		return FAIL;
	}
	return SUCCESS;
}

result_t platform_create_calvinsys(struct node_t *node)
{   android_platform_t* platform;

	platform = (android_platform_t*) node->platform;
	platform->looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

	create_calvinsys(node);
	return SUCCESS;
}

void platform_init(void)
{
#ifdef SLEEP_AT_UNACTIVITY
	last_activity = time(NULL);
#endif
	srand(time(NULL));
}

static int transport_fd_handler(int fd, int events, void *data)
{
	node_t* node = (node_t*) data;
	if (platform_android_handle_data(node, node->transport_client) != SUCCESS) {
		log_error("Error when handling data");
	}
}

static int transport_socket_fd_handler(int fd, int events, void *data)
{
	node_t* node = (node_t*) data;
	if (platform_android_handle_socket_data(node, node->transport_client) != SUCCESS) {
		log_error("Error when handling socket data");
	}
}

void platform_evt_wait(node_t *node, struct timeval *timeout)
{
#ifdef SLEEP_AT_UNACTIVITY
	time_t current_time;
#endif
	android_platform_t* platform;

	int timeout_trigger = 5000;
	int platform_trigger_id = 2, socket_transport_trigger_id = 3;


	platform = (android_platform_t*) node->platform;
	if (node->transport_client == NULL) {
		log_error("tp was null.");
		return;
	}
	// Add FD for FCM and platform communications
	if (ALooper_addFd(platform->looper, ((android_platform_t*) node->platform)->downstream_platform_fd[0], platform_trigger_id, ALOOPER_EVENT_INPUT, &transport_fd_handler, node) != 1) {
		log_error("Could not add fd to looper, looper: %p", platform->looper);
		sleep(5);
	}

	// Only att transport FD if a socket is used it not the same as
	if (node->transport_client->transport_type == TRANSPORT_SOCKET_TYPE) {
		if (ALooper_addFd(platform->looper, ((transport_socket_client_t *)node->transport_client->client_state)->fd, socket_transport_trigger_id, ALOOPER_EVENT_INPUT, &transport_socket_fd_handler, node) != 1) {
			log_error("Could not add socket fd");
			sleep(5);
		}
	}

	int status = ALooper_pollOnce(timeout_trigger, NULL, NULL, NULL);

#ifdef SLEEP_AT_UNACTIVITY
	current_time = time(NULL);
	if (status == ALOOPER_POLL_TIMEOUT) {
		if (difftime(current_time, last_activity) >= ((double) PLATFORM_UNACTIVITY_TIMEOUT)) {
			log("platform timeout triggered");
			api_runtime_serialize_and_stop(node);
		}
	} else {
		last_activity = current_time;
	}
#endif
}

result_t platform_stop(node_t* node)
{
	// Write node stop on pipe, tc will not exist here
	char buffer[6];
	memset(buffer, 0, 6);
	memset(buffer+3, 2 & 0xFF, 1);
	memcpy(buffer+4, RUNTIME_STOP, 2);
	if (write(((android_platform_t*) node->platform)->upstream_platform_fd[1], buffer, 6) < 0) {
		log_error("Failed to write rt started command");
		return FAIL;
	}
	// TODO: Cleanup
	return SUCCESS;
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

#ifdef USE_PERSISTENT_STORAGE
void platform_write_node_state(node_t* node, char *buffer, size_t size)
{
	FILE *fp = NULL;
	char* filename = "calvinconstrained.config";
	char abs_filepath[strlen(filename) + strlen(node->storage_dir) + 1];

	strcpy(abs_filepath, node->storage_dir);
	if (node->storage_dir[strlen(node->storage_dir)-1] != '/')
		strcat(abs_filepath, "/");
	strcat(abs_filepath, filename);

	fp = fopen(abs_filepath, "w+");
	if (fp != NULL) {
		if (fwrite(buffer, 1, size, fp) != size)
			log_error("Failed to write node config");
		fclose(fp);
		log("Wrote node state to disk");
	} else {
		log_error("Failed to open calvinconstrained.config for writing");
		log_error("Errno: %d, error: %s", errno, strerror(errno));
	}
}

result_t platform_read_node_state(node_t* node, char buffer[], size_t size)
{
	FILE *fp = NULL;
	char* filename = "calvinconstrained.config";
	char abs_filepath[strlen(filename) + strlen(node->storage_dir) + 1];

	strcpy(abs_filepath, node->storage_dir);
	if (node->storage_dir[strlen(node->storage_dir)-1] != '/')
		strcat(abs_filepath, "/");
	strcat(abs_filepath, filename);

	fp = fopen(abs_filepath, "r+");
	if (fp != NULL) {
		fread(buffer, 1, size, fp);
		fclose(fp);
		return SUCCESS;
	}
	return FAIL;
}
#endif

