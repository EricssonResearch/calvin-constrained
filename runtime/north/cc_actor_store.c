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
#ifdef CC_ACTOR_IDENTITY
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
#ifdef CC_ACTOR_CAMERA
#include "../../actors/cc_actor_camera.h"
#endif
#ifdef CC_ACTOR_ACCELEROMETER
#include "../../actors/cc_actor_accelerometer.h"
#endif
#ifdef CC_ACTOR_GYROSCOPE
#include "../../actors/cc_actor_gyroscope.h"
#endif
#ifdef CC_ACTOR_PRESSURE
#include "../../actors/cc_actor_pressure.h"
#endif
#ifdef CC_ACTOR_PICKUPGESTURE
#include "../../actors/cc_actor_pickupgesture.h"
#endif

result_t actor_store_init(list_t **actor_types)
{
#ifdef CC_ACTOR_BUTTON
  if (actor_button_register(actor_types) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_IDENTITY
  if (actor_identity_register(actor_types) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_LIGHT
  if (actor_light_register(actor_types) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_SOIL_MOISTURE
  if (actor_soil_moisture_register(actor_types) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_TEMPERATURE
  if (actor_temperature_register(actor_types) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_CAMERA
  if (actor_camera_register(actor_types) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_ACCELEROMETER
  if (actor_accelerometer_register(actor_types) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_GYROSCOPE
  if (actor_gyroscope_register(actor_types) != CC_RESULT_SUCCESS)
    return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_PRESSURE
	if (actor_pressure_register(actor_types) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;
#endif

#ifdef CC_ACTOR_PICKUPGESTURE
	if (actor_pickupgesture_register(actor_types) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;
#endif
  return CC_RESULT_SUCCESS;
}
