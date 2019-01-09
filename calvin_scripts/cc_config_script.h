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
#ifndef CC_CONFIG_SCRIPT_H
#define CC_CONFIG_SCRIPT_H

#include "runtime/north/cc_common.h"

struct cc_calvinsys_obj_t;
struct cc_transport_client_t;
struct cc_node_t;

cc_result_t cc_mpy_calvinsys_object_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);
cc_result_t cc_mpy_calvinsys_object_deserialize(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);
struct cc_transport_client_t *cc_transport_socket_create(struct cc_node_t *node, char *uri);

#define CC_USE_GETOPT (1)
#define CC_USE_SLEEP (0)
#define CC_INACTIVITY_TIMEOUT (5)
#define CC_PYTHON_HEAP_SIZE (100 * 1024)
#define CC_PYTHON_STACK_SIZE (100 * 1024)

#define CC_C_ACTORS

#define _CC_CAPABILITIES \
	{ "io.temperature", cc_mpy_calvinsys_object_open, cc_mpy_calvinsys_object_deserialize, NULL, "\x81\xa4\x64\x61\x74\x61\x96\x01\x02\x03\x04\x05\x06", false, "test.Test" }, \
	{ "http.get", cc_mpy_calvinsys_object_open, cc_mpy_calvinsys_object_deserialize, NULL, NULL, false, "web.http.Command" }, \
	{ "io.stdout", cc_mpy_calvinsys_object_open, cc_mpy_calvinsys_object_deserialize, NULL, NULL, false, "io.Stdout" }, \
	{ "mqtt.subscribe", cc_mpy_calvinsys_object_open, cc_mpy_calvinsys_object_deserialize, NULL, NULL, false, "web.mqtt.Subscribe" }, \
	{ "mqtt.publish", cc_mpy_calvinsys_object_open, cc_mpy_calvinsys_object_deserialize, NULL, NULL, false, "web.mqtt.Publish" }

#define CC_CAPABILITIES _CC_CAPABILITIES

#define CC_TRANSPORTS \
	{ "calvinip", cc_transport_socket_create }, \
	{ "ssdp", cc_transport_socket_create }

#endif
