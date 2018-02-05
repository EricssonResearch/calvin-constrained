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

struct cc_calvinsys_obj_t;
struct cc_transport_client_t;
struct cc_node_t;

cc_result_t cc_test_temperature_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);
cc_result_t cc_test_gpio_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);
struct cc_transport_client_t *cc_transport_socket_create(struct cc_node_t *node, char *uri);

#define CC_USE_GETOPT (1)
#define CC_USE_SLEEP (1)
#define CC_USE_STORAGE (1)

#define _CC_CAPABILITIES \
	{ "io.temperature", cc_test_temperature_open, cc_test_temperature_open, NULL, NULL, false }, \
	{ "io.light", cc_test_gpio_open, cc_test_gpio_open, NULL, "\x82\xa9" "direction" "\xa3" "out" "\xa3" "pin" "\x00", false }, \
	{ "io.button", cc_test_gpio_open, cc_test_gpio_open, NULL, "\x82\xa9" "direction" "\xa2" "in" "\xa3" "pin" "\x01", false }

#define CC_CAPABILITIES _CC_CAPABILITIES

#define CC_TRANSPORTS \
	{ "calvinip", cc_transport_socket_create }, \
	{ "ssdp", cc_transport_socket_create }
