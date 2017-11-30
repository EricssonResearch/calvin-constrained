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
#include <string.h>
#include "cc_actor_store.h"
#ifdef CC_ACTOR_BUTTON
#include "actors/cc_actor_button.h"
#endif
#ifdef CC_ACTOR_IDENTITY
#include "actors/cc_actor_identity.h"
#endif
#ifdef CC_ACTOR_LIGHT
#include "actors/cc_actor_light.h"
#endif
#ifdef CC_ACTOR_SOIL_MOISTURE
#include "actors/cc_actor_soil_moisture.h"
#endif
#ifdef CC_ACTOR_TEMPERATURE
#include "actors/cc_actor_temperature.h"
#endif
#ifdef CC_ACTOR_TRIGGERED_TEMPERATURE
#include "actors/cc_actor_triggered_temperature.h"
#endif
#ifdef CC_ACTOR_CAMERA
#include "actors/cc_actor_camera.h"
#endif
#ifdef CC_ACTOR_ACCELEROMETER
#include "actors/cc_actor_accelerometer.h"
#endif
#ifdef CC_ACTOR_GYROSCOPE
#include "actors/cc_actor_gyroscope.h"
#endif
#ifdef CC_ACTOR_PRESSURE
#include "actors/cc_actor_pressure.h"
#endif
#ifdef CC_ACTOR_PICKUPGESTURE
#include "actors/cc_actor_pickupgesture.h"
#endif
#ifdef CC_ACTOR_STEPCOUNTER
#include "actors/cc_actor_stepcounter.h"
#endif
#ifdef CC_ACTOR_REGISTRY_ATTIBUTE
#include "actors/cc_actor_registry_attribute.h"
#endif
#ifdef CC_ACTOR_TEMPERATURE_TAGGED
#include "actors/cc_actor_temperature_tagged.h"
#endif

cc_result_t cc_actor_store_add(cc_list_t **actor_types, char *name, cc_result_t (*func)(cc_actor_type_t *type))
{
	cc_actor_type_t *type = NULL;

	if (cc_platform_mem_alloc((void **)&type, sizeof(cc_actor_type_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	memset(type, 0, sizeof(cc_actor_type_t));

	if (func(type) != CC_SUCCESS) {
		cc_log_error("Failed to setup '%s'", name);
		cc_platform_mem_free(type);
		return CC_FAIL;
	}

	if (cc_list_add_n(actor_types, name, strlen(name), type, sizeof(cc_actor_type_t *)) == NULL) {
		cc_log_error("Failed to add '%s'", name);
		cc_platform_mem_free(type);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_store_init(cc_list_t **actor_types)
{
#ifdef CC_ACTOR_BUTTON
	if (cc_actor_store_add(actor_types, "io.Button", cc_actor_button_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_IDENTITY
	if (cc_actor_store_add(actor_types, "std.Identity", cc_actor_identity_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_LIGHT
	if (cc_actor_store_add(actor_types, "io.Light", cc_actor_light_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_SOIL_MOISTURE
	if (cc_actor_store_add(actor_types, "sensor.SoilMoisture", cc_actor_soil_moisture_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_TEMPERATURE
	if (cc_actor_store_add(actor_types, "sensor.Temperature", cc_actor_temperature_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_TRIGGERED_TEMPERATURE
	if (cc_actor_store_add(actor_types, "sensor.TriggeredTemperature", cc_actor_triggered_temperature_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_CAMERA
	if (cc_actor_store_add(actor_types, "media.Camera", cc_actor_camera_setup) != CC_ACTOR_STEPCOUNTER)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_ACCELEROMETER
	if (cc_actor_store_add(actor_types, "sensor.TriggeredAccelerometer", cc_actor_accelerometer_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_GYROSCOPE
	if (cc_actor_store_add(actor_types, "sensor.TriggeredGyroscope", cc_actor_gyroscope_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_PRESSURE
	if (cc_actor_store_add(actor_types, "sensor.TriggeredPressure", cc_actor_pressure_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_PICKUPGESTURE
	if (cc_actor_store_add(actor_types, "sensor.PickUpGesture", cc_actor_pickupgesture_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_STEPCOUNTER
	if (cc_actor_store_add(actor_types, "sensor.TriggeredStepCounter", cc_actor_stepcounter_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_REGISTRY_ATTIBUTE
	if (cc_actor_store_add(actor_types, "context.RegistryAttribute", cc_actor_registry_attribute_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

#ifdef CC_ACTOR_TEMPERATURE_TAGGED
	if (cc_actor_store_add(actor_types, "sensor.TemperatureTagged", cc_actor_temperature_tagged_setup) != CC_SUCCESS)
		return CC_FAIL;
#endif

	return CC_SUCCESS;
}
