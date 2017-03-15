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

#ifndef PLATFORM_ANDROID_H
#define PLATFORM_ANDROID_H

#define RUNTIME_STOP "RS"
#define RUNTIME_CALVIN_MSG "CM"
#define RUNTIME_STARTED "RR"
#define FCM_CONNECT "FC"
#define CONNECT_REPLY "FR"
#define RUNTIME_SERIALIZE_AND_STOP "RA"

#define NBR_OF_COMMANDS 4
#define PLATFORM_UNACTIVITY_TIMEOUT 25

typedef struct android_platform_t {
	int upstream_platform_fd[2]; // read end [0], write end [1]
	int downstream_platform_fd[2];
	ALooper* looper;
	result_t (*send_downstream_platform_message)(const struct node_t* node, char* cmd, transport_client_t* tc, char* data, size_t data_size);
	result_t (*send_upstream_platform_message)(const struct node_t* node, char* cmd, transport_client_t* tc, size_t data_size);
	result_t (*read_upstream)(const struct node_t* node, char* buffer, size_t size);
} android_platform_t;

struct platform_command_handler_t {
	char command[50];
	result_t (*handler)(struct node_t* node, char *data, size_t size);
};
#endif
