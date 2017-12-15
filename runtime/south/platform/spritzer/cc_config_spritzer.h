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

#define CC_USE_STORAGE (1)

cc_result_t cc_calvinsys_temp_sensor_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);

#define CC_CAPABILITIES \
	{ "io.temperature", cc_calvinsys_temp_sensor_open, NULL, NULL, NULL }

struct cc_actor_type_t;

cc_result_t cc_actor_identity_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_temperature_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_triggered_temperature_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_temperature_tagged_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_registry_attribute_setup(struct cc_actor_type_t *type);

#define CC_C_ACTORS \
	{ "std.Identity", cc_actor_identity_setup }, \
	{ "sensor.Temperature", cc_actor_temperature_setup }, \
	{ "sensor.TriggeredTemperature", cc_actor_triggered_temperature_setup }, \
  { "sensor.TemperatureTagged", cc_actor_temperature_tagged_setup }, \
	{ "context.RegistryAttribute", cc_actor_registry_attribute_setup }

struct cc_transport_client_t;
struct cc_node_t;

struct cc_transport_client_t *cc_transport_spritzer_create(struct cc_node_t *node, char *uri);

#define CC_TRANSPORTS \
	{ "calvinip", cc_transport_spritzer_create }