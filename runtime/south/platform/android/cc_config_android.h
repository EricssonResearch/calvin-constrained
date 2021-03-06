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
#ifndef CC_CONFIG_ANDROID_H
#define CC_CONFIG_ANDROID_H

#include "runtime/north/cc_common.h"

#define CC_USE_STORAGE (1)

struct cc_actor_type_t;
struct cc_transport_client_t;
struct cc_node_t;

cc_result_t cc_actor_accelerometer_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_gyroscope_setup(struct cc_actor_type_t *type);
struct cc_transport_client_t *cc_transport_socket_create(struct cc_node_t *node, char *uri);
struct cc_transport_client_t *cc_transport_fcm_create(struct cc_node_t *node, char *uri);

#define CC_C_ACTORS \
  { "sensor.Accelerometer", cc_actor_accelerometer_setup }, \
	{ "sensor.Gyroscope", cc_actor_gyroscope_setup }

#define CC_TRANSPORTS \
	{ "calvinip", cc_transport_socket_create }, \
	{ "ssdp", cc_transport_socket_create }, \
	{ "fcm", cc_transport_fcm_create }

#endif /* CC_CONFIG_ANDROID_H */
