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
#include <unistd.h>
#include <android/log.h>
#include <platform/android/calvinsys/cc_accelerometer.h>
#include <platform/android/calvinsys/cc_gyroscope.h>
#include <platform/android/calvinsys/cc_pressure.h>
#include <platform/android/calvinsys/cc_pickupgesture.h>
#include <platform/android/calvinsys/cc_stepcounter.h>
#include "cc_api.h"
#include "runtime/south/platform/android/cc_platform_android.h"
#include "runtime/south/transport/socket/cc_transport_socket.h"
#include "runtime/north/cc_proto.h"
#include "runtime/north/coder/cc_coder.h"
#include "msgpuck/msgpuck.h"
#include "runtime/south/transport/fcm/cc_transport_fcm.h"
#include "runtime/south/platform/cc_platform.h"
#include "actors/cc_actor_accelerometer.h"

static cc_result_t command_transport_connected(cc_node_t *node, char *payload_data, size_t size);
static cc_result_t command_rt_stop(cc_node_t *node, char *payload_data, size_t size);
static cc_result_t command_calvin_msg(cc_node_t *node, char *payload_data, size_t size);
static cc_result_t platform_serialize_and_stop(cc_node_t *node, char *payload_data, size_t size);
static cc_result_t platform_trigger_reconnect(cc_node_t *node, char *payload_data, size_t size);
static cc_result_t platform_register_external_calvinsys(cc_node_t *node, char *payload_data, size_t size);
static cc_result_t platform_handle_external_calvinsys_data(cc_node_t *node, char *payload_data, size_t size);

struct platform_command_handler_t platform_command_handlers[PLATFORM_ANDROID_NBR_OF_COMMANDS] = {
	{PLATFORM_ANDROID_CONNECT_REPLY, command_transport_connected},
	{PLATFORM_ANDROID_RUNTIME_STOP, command_rt_stop},
	{PLATFORM_ANDROID_RUNTIME_CALVIN_MSG, command_calvin_msg},
	{PLATFORM_ANDROID_RUNTIME_SERIALIZE_AND_STOP, platform_serialize_and_stop},
	{PLATFORM_ANDROID_RUNTIME_TRIGGER_RECONNECT, platform_trigger_reconnect},
	{PLATFORM_ANDROID_REGISTER_EXTERNAL_CALVINSYS, platform_register_external_calvinsys},
	{PLATFORM_ANDROID_EXTERNAL_CALVINSYS_PAYLOAD, platform_handle_external_calvinsys_data}
};

typedef struct platform_android_external_state_t {
	char *data;
	size_t data_size;
} platform_android_external_state_t;

static int send_upstream_platform_message(const cc_node_t *node, char *cmd, char *data, size_t data_size)
{
	int res = 0;

	cc_transport_set_length_prefix(data, data_size - CC_TRANSPORT_LEN_PREFIX_SIZE);
	memcpy(data + CC_TRANSPORT_LEN_PREFIX_SIZE, cmd, PLATFORM_ANDROID_COMMAND_SIZE); // set 2 byte command type
	res = write(((android_platform_t *)node->platform)->upstream_platform_fd[1], data, data_size);
	if (res == data_size) {
		cc_log_debug("Wrote upstream message of size '%d'", res);
		return res;
	}

	cc_log_error("Could not write upstream message of size '%d', res '%d'", data_size, res);

	return res;
}

static cc_result_t send_downstream_platform_message(const cc_node_t *node, char *data, size_t data_size)
{
	int res = 0;

	res = write(((android_platform_t *)node->platform)->downstream_platform_fd[1], data, data_size);
	if (res > 0) {
		cc_log_debug("Wrote downstream message with size %d", data_size);
		return CC_SUCCESS;
	}

	cc_log_error("Could not write downstream message, result: %d", res);
	return CC_FAIL;
}

static cc_result_t read_upstream(const cc_node_t *node, char *buffer, size_t size)
{
	int bytes_read = 0, status = 0, fd = ((android_platform_t *)node->platform)->upstream_platform_fd[0];
	unsigned int datasize = 0;
	fd_set set;

	FD_ZERO(&set);
	FD_SET(fd, &set);

	status = select(fd + 1, &set, NULL, NULL, NULL);
	if (status > 0) {
		bytes_read = read(fd, buffer, CC_TRANSPORT_LEN_PREFIX_SIZE);
		if (bytes_read < CC_TRANSPORT_LEN_PREFIX_SIZE) {
			cc_log_error("Failed to read message size '%d'", bytes_read);
			return CC_FAIL;
		}

		datasize = cc_transport_get_message_len(buffer);
		if (datasize > size - CC_TRANSPORT_LEN_PREFIX_SIZE) {
			cc_log_error("Error when reading upstream data, read buffer '%d' < message '%d'.", size, datasize);
			return CC_FAIL;
		}

		bytes_read = read(fd, buffer + CC_TRANSPORT_LEN_PREFIX_SIZE, datasize);
		if (bytes_read != datasize) {
			cc_log_error("Error when reading from pipe, expected '%d', got '%d'", datasize, bytes_read);
			return CC_FAIL;
		}
	} else if (status < 0) {
		cc_log_error("Error on upstream select");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static cc_result_t handle_platform_call(cc_node_t *node, int fd)
{
	char size_buffer[4];
	size_t total_msg_size;
	int i;

	read(fd, size_buffer, 4);
	total_msg_size = cc_transport_get_message_len(size_buffer);
	char data_buffer[total_msg_size];

	memcpy(data_buffer, size_buffer, 4);
	read(fd, data_buffer+4, total_msg_size);

	// Handle command
	for (i = 0; i < PLATFORM_ANDROID_NBR_OF_COMMANDS; i++) {
		if (strncmp(platform_command_handlers[i].command, data_buffer+4, 2) == 0) {
			cc_log_debug("will handle command %.2s from pipe", data_buffer+4);
			platform_command_handlers[i].handler(node, data_buffer + 6, total_msg_size - 2);
			return CC_SUCCESS;
		}
	}
	cc_log_error("Command %.2s not found", data_buffer+4);
	return CC_FAIL;
}

static cc_result_t platform_android_handle_data(cc_node_t *node, cc_transport_client_t *transport_client)
{
	cc_result_t result = handle_platform_call(node, ((android_platform_t *)node->platform)->downstream_platform_fd[0]);

	if (result != CC_SUCCESS)
		cc_log_error("fcm_handle platform call failed");
	return result;
}

static cc_result_t platform_android_handle_socket_data(cc_node_t *node, cc_transport_client_t *transport_client)
{
	return cc_transport_handle_data(node, transport_client, cc_node_handle_message);
}

static int platform_android_fcm_fd_handler(int fd, int events, void *data)
{
	cc_node_t *node = (cc_node_t *)data;

	if (platform_android_handle_data(node, node->transport_client) != CC_SUCCESS)
		cc_log_error("Error when handling data");

	return 1; // Always return 1, since 0  will unregister the FD in the looper
}

static int platform_android_socket_fd_handler(int fd, int events, void *data)
{
	cc_node_t *node = (cc_node_t *)data;

	if (platform_android_handle_socket_data(node, node->transport_client) != CC_SUCCESS)
		cc_log_error("Error when handling socket data");

	return 1; // Always return 1, since 0  will unregister the FD in the looper
}

#if (CC_USE_SLEEP == 1)
void cc_platform_deepsleep(cc_node_t *node)
{
	cc_log("Going to deepsleep state, runtime will stop!");
}
#endif

cc_platform_evt_wait_status_t cc_platform_evt_wait(cc_node_t *node, uint32_t timeout_seconds)
{
	android_platform_t *platform = (android_platform_t *)node->platform;
	int poll_result;

	// Add FD for FCM and platform communications
	if (ALooper_addFd(platform->looper, ((android_platform_t *) node->platform)->downstream_platform_fd[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, &platform_android_fcm_fd_handler, node) != 1) {
		cc_log_error("Could not add fd to looper, looper: %p", platform->looper);
		sleep(5);
	}

	// Only att transport FD if a socket is used it not the same as
	if (node->transport_client != NULL && (node->transport_client->state == CC_TRANSPORT_PENDING || node->transport_client->state == CC_TRANSPORT_ENABLED)) {
		if (node->transport_client->transport_type == CC_TRANSPORT_SOCKET_TYPE) {
			if (ALooper_addFd(platform->looper,
				((cc_transport_socket_client_t *)node->transport_client->client_state)->fd,
				ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT,
				&platform_android_socket_fd_handler, node) != 1) {
				cc_log_error("Could not add socket fd");
				sleep(5);
			}
		}
	} else {
		sleep(10);
		return CC_PLATFORM_EVT_WAIT_TIMEOUT;
	}

	poll_result = ALooper_pollOnce(timeout_seconds == CC_INDEFINITELY_TIMEOUT ? -1 : timeout_seconds, NULL, NULL, NULL);
	if (poll_result == ALOOPER_POLL_CALLBACK || poll_result == ALOOPER_POLL_TIMEOUT)
		return CC_PLATFORM_EVT_WAIT_DATA_READ;

	return CC_PLATFORM_EVT_WAIT_TIMEOUT;
}

void cc_platform_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	__android_log_vprint(ANDROID_LOG_VERBOSE, "Calvin Constrained Android", fmt, args);
	va_end(args);
}

static cc_result_t command_calvin_msg(cc_node_t *node, char *payload_data, size_t size)
{
	unsigned int len = cc_transport_get_message_len(payload_data);

	return cc_node_handle_message(node, payload_data + 4, len);
}

static cc_result_t command_transport_connected(cc_node_t *node, char *data, size_t size)
{
	node->transport_client->state = CC_TRANSPORT_CONNECTED;
	return CC_SUCCESS;
}

static cc_result_t command_rt_stop(cc_node_t *node, char *payload_data, size_t size)
{
	return cc_api_runtime_stop(node);
}

static cc_result_t platform_serialize_and_stop(cc_node_t *node, char *data, size_t size)
{
	return cc_api_runtime_serialize_and_stop(node);
}

static cc_result_t platform_trigger_reconnect(cc_node_t *node, char *data, size_t size)
{
	return cc_api_reconnect(node);
}

static cc_result_t node_setup_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	// TODO: Return success or fail of the registration.
	return CC_SUCCESS;
}

static bool platform_external_can_write(struct cc_calvinsys_obj_t *obj)
{
	return obj->handler->calvinsys->node->transport_client != NULL;
}

static cc_result_t platform_external_write(struct cc_calvinsys_obj_t *obj, char *data, size_t data_size)
{
	char buffer[500+data_size], *w = buffer + 6;

	w = cc_coder_encode_map(w, 3);
	w = cc_coder_encode_kv_uint(w, "id", obj->id);
	w = cc_coder_encode_kv_bin(w, "payload", data, data_size);

	if (send_upstream_platform_message(obj->handler->calvinsys->node, PLATFORM_ANDROID_EXTERNAL_CALVINSYS_PAYLOAD, buffer, w - buffer) > 0)
		return CC_SUCCESS;

	return CC_FAIL;
}

static bool platform_external_can_read(struct cc_calvinsys_obj_t *obj)
{
	platform_android_external_state_t *state = (platform_android_external_state_t *)obj->state;

	return state->data != NULL && state->data_size > 0;
}

static cc_result_t platform_external_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	platform_android_external_state_t *state = (platform_android_external_state_t *)obj->state;

	if (state->data != NULL && state->data_size > 0) {
		*data = state->data;
		*size = state->data_size;
		state->data = NULL;
		state->data_size = 0;
		return CC_SUCCESS;
	}

	cc_log_error("No data available");

	return CC_FAIL;
}

static cc_result_t platform_external_close(cc_calvinsys_obj_t *obj)
{
	platform_android_external_state_t *state = (platform_android_external_state_t *)obj->state;
	char buffer[500], *w;

	w = buffer+6;
	w = cc_coder_encode_map(w, 2);
	w = cc_coder_encode_kv_uint(w, "id", obj->id);

	if (send_upstream_platform_message(obj->handler->calvinsys->node, PLATFORM_ANDROID_DESTROY_EXTERNAL_CALVINSYS, buffer, w-buffer) > 0) {
		if (state->data != NULL)
			cc_platform_mem_free((void *)state->data);
		cc_platform_mem_free((void *)state);
		return CC_SUCCESS;
	}

	return CC_FAIL;
}

static cc_calvinsys_obj_t *platform_external_open(cc_calvinsys_handler_t *handler, char *data, size_t len, void *handler_state, uint32_t id, const char *capability_name)
{
	cc_calvinsys_obj_t *obj = NULL;
	platform_android_external_state_t *state = NULL;
	char buffer[500], *w = NULL;

	memset(buffer, 0, 500);

	if (cc_platform_mem_alloc((void **)&obj, sizeof(cc_calvinsys_obj_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	if (cc_platform_mem_alloc((void **)&state, sizeof(platform_android_external_state_t))) {
		cc_log_error("Failed to allocate memory");
		cc_platform_mem_free((void *)obj);
		return NULL;
	}

	state->data = NULL;
	state->data_size = 0;
	obj->can_write = platform_external_can_write;
	obj->write = platform_external_write;
	obj->can_read = platform_external_can_read;
	obj->read = platform_external_read;
	obj->close = platform_external_close;
	obj->handler = handler;
	obj->state = state;
	obj->id = id;

	if (handler->objects == NULL)
		cc_log_error("Did not add object to handler!");

	// Give it its ID
	w = buffer + 6;
	w = cc_coder_encode_map(w, 2);
	w = cc_coder_encode_kv_str(w, "name", capability_name, strlen(capability_name));
	w = cc_coder_encode_kv_uint(w, "id", id);

	if (send_upstream_platform_message(handler->calvinsys->node, PLATFORM_ANDROID_OPEN_EXTERNAL_CALVINSYS, buffer, w-buffer) > 0)
		return obj;

	cc_platform_mem_free((void *)state);
	cc_platform_mem_free((void *)obj);

	return NULL;
}

// TODO: Add functionality to let third party apps unregister their capability. However, what do we do with the actors that depend on that calvinsys?
static cc_result_t platform_register_external_calvinsys(cc_node_t *node, char *data, size_t data_size)
{
	cc_calvinsys_handler_t *handler = NULL;
	char *name;

	if (cc_platform_mem_alloc((void **)&handler, sizeof(cc_calvinsys_handler_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&name, data_size + 1) != CC_SUCCESS) {
		cc_log_error("Could not allocate memory for the name of the calvinsys");
		cc_platform_mem_free(handler);
		return CC_FAIL;
	}

	memset(name, 0, data_size+1); // Add null char at the end of the name
	memcpy(name, data, data_size);

	handler->open = platform_external_open;
	handler->objects = NULL;
	handler->next = NULL;

	cc_calvinsys_add_handler(&node->calvinsys, handler);
	handler->calvinsys->node = node;

	// TODO: Send reply to third party app, reply false if capability already is registered
	if (cc_calvinsys_register_capability(node->calvinsys, name, handler, NULL) == CC_SUCCESS)
		return cc_proto_send_node_setup(node, node_setup_reply_handler);

	return CC_FAIL;
}

static cc_result_t platform_handle_external_calvinsys_data(cc_node_t *node, char *data, size_t size)
{
	char *name = NULL, *payload = NULL;
	int name_len, payload_len, id;
	cc_calvinsys_handler_t *handler = NULL;
	platform_android_external_state_t *state = NULL;
	cc_calvinsys_obj_t *object = NULL;

	if (cc_coder_decode_string_from_map(data, "calvinsys", &name, &name_len) != CC_SUCCESS) {
		cc_log_error("Could not parse calvinsys key.");
		return CC_FAIL;
	}

	if (cc_coder_decode_bin_from_map(data, "payload", &payload, &payload_len) != CC_SUCCESS) {
		cc_log_error("Could not parse paylaod key");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(data, "id", &id) != CC_SUCCESS) {
		cc_log_error("Could not parse id from map when handling calvinsys data");
		return CC_FAIL;
	}

	handler = ((cc_calvinsys_capability_t *) cc_list_get(node->calvinsys->capabilities, name))->handler;
	if (handler == NULL) {
		cc_log_error("Could not find calvinsys '%s'", name);
		return CC_FAIL;
	}

	if (cc_calvinsys_get_obj_by_id(&object, handler, id) != CC_SUCCESS) {
		cc_log_error("Could not find calvinsys object for that handler");
		return CC_FAIL;
	}

	state = object->state;

	if (state->data != NULL)
		cc_platform_mem_free((void *)state->data);

	if (cc_platform_mem_alloc((void **)&state->data, payload_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memcpy(state->data, payload, payload_len);
	state->data_size = payload_len;

	return CC_SUCCESS;
}

cc_result_t cc_platform_create(cc_node_t *node)
{
	android_platform_t *platform = NULL;

	if (cc_platform_mem_alloc((void **)&platform, sizeof(android_platform_t)) != CC_SUCCESS) {
		cc_log_error("Could not allocate memory for platform object.");
		return CC_FAIL;
	}

	platform->send_downstream_platform_message = send_downstream_platform_message;
	platform->send_upstream_platform_message = send_upstream_platform_message;
	platform->read_upstream = read_upstream;
	node->platform = platform;

	if (pipe(((android_platform_t *)node->platform)->upstream_platform_fd) < 0 ||
			pipe(((android_platform_t *)node->platform)->downstream_platform_fd) < 0) {
		cc_log_error("Could not open pipes for transport");
		cc_platform_mem_free(platform);
		return CC_FAIL;
	}

	cc_log("platform fd up; %d %d, down: %d %d", ((android_platform_t *)node->platform)->upstream_platform_fd[0],
	    ((android_platform_t *)node->platform)->upstream_platform_fd[1],
	    ((android_platform_t *)node->platform)->downstream_platform_fd[0],
	    ((android_platform_t *)node->platform)->downstream_platform_fd[1]);

	return CC_SUCCESS;
}

cc_result_t cc_platform_node_started(struct cc_node_t *node)
{
	// Send RT started, tc is not created here, so just write on the pipe
	char buffer[6];

	memset(buffer, 0, 6);
	memset(buffer + 3, 2 & 0xFF, 1);
	memcpy(buffer + 4, PLATFORM_ANDROID_RUNTIME_STARTED, 2);

	if (write(((android_platform_t *)node->platform)->upstream_platform_fd[1], buffer, 6) < 0) {
		cc_log_error("Failed to write rt started command");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static cc_result_t init_sensor_by_type(ASensor *sensor, cc_calvinsys_t **calvinsys)
{
	const char *type = ASensor_getStringType(sensor);
	bool wakeup = ASensor_isWakeUpSensor(sensor);

	cc_log("Sensor %s found. Is wakeup?: %d", type, wakeup);

	if (strcmp(type, "android.sensor.pressure") == 0) {
		if (calvinsys_pressure_create(calvinsys, "io.pressure") != CC_SUCCESS) {
			cc_log_error("Could not create sensor %s calvinsys", type);
			return CC_FAIL;
		}
	} else if(strcmp(type, "android.sensor.pick_up_gesture") == 0) {
		if (calvinsys_pickupgesture_create(calvinsys, "io.pickupgesture") != CC_SUCCESS) {
			cc_log_error("Could not create sensor %s calvinsys", type);
			return CC_FAIL;
		}
	} else if(strcmp(type, "android.sensor.step_counter") == 0) {
		if (calvinsys_stepcounter_create(calvinsys, "io.stepcounter") != CC_SUCCESS) {
			cc_log_error("Could not create sensor %s calvinsys", type);
			return CC_FAIL;
		}
	}

	return CC_SUCCESS;
}

ASensor *get_sensor_by_name(const char *name)
{
	ASensorManager *mgr;
	ASensorList sensor_list;
	int i, sensor_count;

	mgr = ASensorManager_getInstance();
	sensor_count = ASensorManager_getSensorList(mgr, &sensor_list);

	for (i = 0; i < sensor_count; i++) {
		const char *type = ASensor_getStringType(sensor_list[i]);

		if (strcmp(type, name) == 0)
			return (ASensor *)sensor_list[i];
	}

	return NULL;
}

cc_result_t cc_platform_add_capabilities(cc_calvinsys_t *calvinsys)
{
	ASensorManager *mgr;
	ASensorList sensor_list;
	int i, sensor_count;

	android_platform_t *platform = (android_platform_t *)calvinsys->node->platform;

	platform->looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

	if (calvinsys_accelerometer_create(calvinsys, "io.accelerometer") != CC_SUCCESS) {
		cc_log_error("Could not create sensor accelerometer calvinsys");
		return CC_FAIL;
	}

	if (calvinsys_gyroscope_create(calvinsys, "io.gyroscope") != CC_SUCCESS) {
		cc_log_error("Could not create sensor gyroscope calvinsys");
		return CC_FAIL;
	}

	// Find other sensors that does not have a ASENSOR_TYPE_* definition in the NDK
	mgr = ASensorManager_getInstance();
	sensor_count = ASensorManager_getSensorList(mgr, &sensor_list);

	for (i = 0; i < sensor_count; i++)
		init_sensor_by_type((ASensor *)sensor_list[i], calvinsys);

	return CC_SUCCESS;
}

void cc_platform_init(void)
{
	srand(time(NULL));
}

cc_result_t cc_platform_stop(cc_node_t *node)
{
	// Write node stop on pipe, tc will not exist here
	char buffer[6];

	memset(buffer, 0, 6);
	memset(buffer + 3, 2 & 0xFF, 1);
	memcpy(buffer + 4, PLATFORM_ANDROID_RUNTIME_STOP, 2);

	if (write(((android_platform_t *)node->platform)->upstream_platform_fd[1], buffer, 6) < 0) {
		cc_log_error("Failed to write rt started command");
		return CC_FAIL;
	}

	// TODO: Cleanup
	return CC_SUCCESS;
}

cc_result_t cc_platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		cc_log_error("Failed to allocate '%ld' memory", (unsigned long)size);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

void *cc_platform_mem_calloc(size_t nitems, size_t size)
{
	void *ptr = NULL;

	if (cc_platform_mem_alloc(&ptr, nitems * size) != CC_SUCCESS)
		return NULL;

	memset(ptr, 0, nitems * size);
	return ptr;
}

void cc_platform_mem_free(void *buffer)
{
	free(buffer);
}

uint32_t cc_platform_get_seconds(cc_node_t *node)
{
	struct timeval value;

	gettimeofday(&value, NULL);

	return value.tv_sec;
}

#if (CC_USE_STORAGE == 1)
void cc_platform_write_node_state(cc_node_t *node, char *buffer, size_t size)
{
	FILE *fp = NULL;
	char abs_filepath[strlen(CC_STATE_FILE) + strlen(node->storage_dir) + 1];

	strcpy(abs_filepath, node->storage_dir);
	if (node->storage_dir[strlen(node->storage_dir) - 1] != '/')
		strcat(abs_filepath, "/");
	strcat(abs_filepath, CC_STATE_FILE);

	fp = fopen(abs_filepath, "w+");
	if (fp != NULL) {
		if (fwrite(buffer, 1, size, fp) != size)
			cc_log_error("Failed to write node config");
		else
			cc_log("Wrote node state to disk");
		fclose(fp);
	} else {
		cc_log_error("Failed to open %s for writing", CC_STATE_FILE);
		cc_log_error("Errno: %d, error: %s", errno, strerror(errno));
	}
}

cc_result_t cc_platform_read_node_state(cc_node_t *node, char buffer[], size_t size)
{
	cc_result_t result = CC_FAIL;
	FILE *fp = NULL;
	char abs_filepath[strlen(CC_STATE_FILE) + strlen(node->storage_dir) + 1];
	int len = 0;

	strcpy(abs_filepath, node->storage_dir);
	if (node->storage_dir[strlen(node->storage_dir)-1] != '/')
		strcat(abs_filepath, "/");
	strcat(abs_filepath, CC_STATE_FILE);

	fp = fopen(abs_filepath, "r+");
	if (fp != NULL) {
		len = fread(buffer, 1, size, fp);
		if (len > 0)
			result = CC_SUCCESS;
		fclose(fp);
	}

	return result;
}
#endif
