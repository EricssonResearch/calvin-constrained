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
#ifndef CC_CONFIG_X86_H
#define CC_CONFIG_X86_H

#include "runtime/north/cc_common.h"

#define CC_USE_GETOPT (1)
#define CC_USE_SLEEP (1)
#define CC_USE_STORAGE (1)

struct cc_calvinsys_obj_t;
struct cc_actor_type_t;
struct cc_transport_client_t;
struct cc_node_t;

cc_result_t cc_test_temperature_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);
cc_result_t cc_test_gpio_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);
cc_result_t cc_actor_identity_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_button_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_light_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_soil_moisture_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_temperature_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_triggered_temperature_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_temperature_tagged_setup(struct cc_actor_type_t *type);
cc_result_t cc_actor_registry_attribute_setup(struct cc_actor_type_t *type);
struct cc_transport_client_t *cc_transport_socket_create(struct cc_node_t *node, char *uri);

#define CC_CAPABILITIES \
	{ "io.temperature", cc_test_temperature_open, cc_test_temperature_open, NULL, NULL, false }, \
	{ "io.light", cc_test_gpio_open, cc_test_gpio_open, NULL, "\x82\xa9" "direction" "\xa3" "out" "\xa3" "pin" "\x00", false }, \
	{ "io.button", cc_test_gpio_open, cc_test_gpio_open, NULL, "\x82\xa9" "direction" "\xa2" "in" "\xa3" "pin" "\x01", false }

#define CC_C_ACTORS \
	{ "std.Identity", cc_actor_identity_setup }, \
	{ "io.Button", cc_actor_button_setup }, \
	{ "io.Light", cc_actor_light_setup }, \
  { "sensor.SoilMoisture", cc_actor_soil_moisture_setup }, \
	{ "sensor.Temperature", cc_actor_temperature_setup }, \
	{ "sensor.TriggeredTemperature", cc_actor_triggered_temperature_setup }, \
  { "sensor.TemperatureTagged", cc_actor_temperature_tagged_setup }, \
	{ "context.RegistryAttribute", cc_actor_registry_attribute_setup }

#define CC_TRANSPORTS \
	{ "calvinip", cc_transport_socket_create }, \
	{ "ssdp", cc_transport_socket_create }

#endif /* CC_CONFIG_X86_H */
