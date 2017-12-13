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
#include "runtime/north/cc_common.h"

#define CC_USE_GETOPT (1)
#define CC_USE_SLEEP (1)
#define CC_USE_STORAGE (1)
#define CC_USE_PYTHON (1)

struct cc_calvinsys_obj_t;

cc_result_t cc_platformx86_temp_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);
cc_result_t cc_platformx86_gpio_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);

#define CC_CAPABILITIES \
	{ "io.temperature", cc_platformx86_temp_open, NULL, NULL, NULL }, \
	{ "io.light", cc_platformx86_gpio_open, NULL, NULL, "\x82\xa9\x64\x69\x72\x65\x63\x74\x69\x6f\x6e\xa3\x6f\x75\x74\xa3\x70\x69\x6e\x00" }, \
	{ "io.button", cc_platformx86_gpio_open, NULL, NULL, "\x82\xa9\x64\x69\x72\x65\x63\x74\x69\x6f\x6e\xa2\x69\x6e\xa3\x70\x69\x6e\x01" }

struct cc_transport_client_t;
struct cc_node_t;

struct cc_transport_client_t *cc_transport_socket_create(struct cc_node_t *node, char *uri);

#define CC_TRANSPORTS \
	{ "calvinip", cc_transport_socket_create }, \
	{ "ssdp", cc_transport_socket_create }
