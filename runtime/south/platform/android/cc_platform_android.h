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
#include <android/sensor.h>
#include "../../../north/cc_transport.h"

#ifndef PLATFORM_ANDROID_H
#define PLATFORM_ANDROID_H

#define PLATFORM_ANDROID_COMMAND_SIZE 								2
#define PLATFORM_ANDROID_CONNECT_REPLY								"FR"
#define PLATFORM_ANDROID_RUNTIME_STOP									"RS"
#define PLATFORM_ANDROID_RUNTIME_CALVIN_MSG						"CM"
#define PLATFORM_ANDROID_RUNTIME_STARTED							"RR"
#define PLATFORM_ANDROID_FCM_CONNECT									"FC"
#define PLATFORM_ANDROID_RUNTIME_SERIALIZE_AND_STOP		"RA"
#define PLATFORM_ANDROID_RUNTIME_TRIGGER_RECONNECT		"RC"
#define PLATFORM_ANDROID_REGISTER_EXTERNAL_CALVINSYS	"CS"
#define PLATFORM_ANDROID_DESTROY_EXTERNAL_CALVINSYS		"CD"
#define PLATFORM_ANDROID_OPEN_EXTERNAL_CALVINSYS			"CI"
#define PLATFORM_ANDROID_EXTERNAL_CALVINSYS_PAYLOAD		"CP"
#define PLATFORM_ANDROID_NBR_OF_COMMANDS							7
#define PLATFORM_ANDROID_UNACTIVITY_TIMEOUT						300

#define CC_ANDROID_LOOPER_CALLBACK_RESULT_CONTINUE 1
#define CC_ANDROID_LOOPER_CALLBACK_RESULT_UNREGISTER 0

typedef struct android_platform_t {
	int upstream_platform_fd[2]; // read end [0], write end [1]
	int downstream_platform_fd[2];
	ALooper *looper;
	int (*send_upstream_platform_message)(const struct node_t *node, char *cmd, char *data, size_t data_size);
	result_t (*send_downstream_platform_message)(const struct node_t *node, char *data, size_t data_size);
	result_t (*read_upstream)(const struct node_t *node, char *buffer, size_t size);
} android_platform_t;

struct platform_command_handler_t {
	char command[PLATFORM_ANDROID_COMMAND_SIZE];
	result_t (*handler)(struct node_t *node, char *data, size_t size);
};

typedef struct android_sensor_data_t {
	ASensorEventQueue *queue;
	ASensor *sensor;
	ASensorEvent *event;
	char *data;
	size_t data_size;
} android_sensor_data_t;

#endif
