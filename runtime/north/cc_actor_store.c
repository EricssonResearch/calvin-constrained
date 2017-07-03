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
#include "cc_actor_store.h"
#ifdef CC_ACTOR_BUTTON
#include "../../actors/cc_actor_button.h"
#endif
#ifdef CC_ACTOR_IDENTIY
#include "../../actors/cc_actor_identity.h"
#endif
#ifdef CC_ACTOR_LIGHT
#include "../../actors/cc_actor_light.h"
#endif
#ifdef CC_ACTOR_SOIL_MOISTURE
#include "../../actors/cc_actor_soil_moisture.h"
#endif
#ifdef CC_ACTOR_TEMPERATURE
#include "../../actors/cc_actor_temperature.h"
#endif

result_t actor_store_init(list_t **actor_types)
{
  actor_type_t *type = NULL;

#ifdef CC_ACTOR_BUTTON
  if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return CC_RESULT_FAIL;
  }

  type->init = actor_button_init;
  type->set_state = actor_button_set_state;
  type->free_state = actor_button_free;
  type->fire_actor = actor_button_fire;
  type->get_managed_attributes = NULL;
  type->will_migrate = NULL;
  type->will_end = NULL;
  type->did_migrate = NULL;

  if (list_add_n(actor_types, "io.Button", 9, type, sizeof(actor_type_t *)) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_IDENTIY
  if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return CC_RESULT_FAIL;
  }

  type->init = actor_identity_init;
  type->set_state = actor_identity_set_state;
  type->free_state = actor_identity_free;
  type->fire_actor = actor_identity_fire;
  type->get_managed_attributes = actor_identity_get_managed_attributes;
  type->will_migrate = NULL;
  type->will_end = NULL;
  type->did_migrate = NULL;

  if (list_add_n(actor_types, "std.Identity", 12, type, sizeof(actor_type_t *)) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_LIGHT
  if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return CC_RESULT_FAIL;
  }

  type->init = actor_light_init;
  type->set_state = actor_light_set_state;
  type->free_state = actor_light_free;
  type->fire_actor = actor_light_fire;
  type->get_managed_attributes = NULL;
  type->will_migrate = NULL;
  type->will_end = NULL;
  type->did_migrate = NULL;

  if (list_add_n(actor_types, "io.Light", 8, type, sizeof(actor_type_t *)) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_SOIL_MOISTURE
  if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return CC_RESULT_FAIL;
  }

  type->init = actor_soil_moisture_init;
  type->set_state = actor_soil_moisture_set_state;
  type->free_state = actor_soil_moisture_free;
  type->fire_actor = actor_soil_moisture_fire;
  type->get_managed_attributes = NULL;
  type->will_migrate = NULL;
  type->will_end = NULL;
  type->did_migrate = NULL;

  if (list_add_n(actor_types, "io.TriggeredSoilMoisture", 24, type, sizeof(actor_type_t *)) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_TEMPERATURE
  if (platform_mem_alloc((void **)&type, sizeof(actor_type_t)) != CC_RESULT_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return CC_RESULT_FAIL;
  }

  type->init = actor_temperature_init;
  type->set_state = actor_temperature_set_state;
  type->free_state = actor_temperature_free;
  type->fire_actor = actor_temperature_fire;
  type->get_managed_attributes = NULL;
  type->will_migrate = NULL;
  type->will_end = NULL;
  type->did_migrate = NULL;

  if (list_add_n(actor_types, "sensor.TriggeredTemperature", 27, type, sizeof(actor_type_t *)) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

  return CC_RESULT_SUCCESS;
}
