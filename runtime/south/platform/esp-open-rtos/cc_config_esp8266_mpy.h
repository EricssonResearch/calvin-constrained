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
struct cc_actor_type_t;
struct cc_transport_client_t;
struct cc_node_t;

cc_result_t cc_calvinsys_ds18b20_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);
cc_result_t cc_calvinsys_gpio_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);
cc_result_t cc_calvinsys_yl69_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);
struct cc_transport_client_t *cc_transport_socket_create(struct cc_node_t *node, char *uri);

#define CC_USE_SLEEP (0)
#define CC_USE_STORAGE (1)
#define CC_USE_WIFI_AP (1)
#define CC_WIFI_AP_SSID "calvin-esp"
#define CC_WIFI_AP_PSK "calvin-esp"
#define CC_PYTHON_HEAP_SIZE (10 * 1024)

#define CC_CAPABILITIES \
	{ "io.temperature", cc_calvinsys_ds18b20_open, NULL, NULL, "\x81\xa3" "pin" "\x05", false }, \
	{ "io.light", cc_calvinsys_gpio_open, NULL, NULL, "\x82\xa9" "direction" "\xa3" "out" "\xa3" "pin" "\x00", false }, \
	{ "io.soil_moisture", cc_calvinsys_yl69_open, NULL, NULL, NULL, false }

#define CC_TRANSPORTS \
	{ "calvinip", cc_transport_socket_create }, \
	{ "ssdp", cc_transport_socket_create }
