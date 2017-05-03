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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <android/sensor.h>
#include "accelerometer.h"
#include "../../../../../runtime/south/platform/cc_platform.h"
#include "../../../../../runtime/south/platform/android/cc_platform_android.h"
#include "../../../../../msgpuck/msgpuck.h"
#include "../../../../../calvinsys/cc_calvinsys.h"
#include "../../../../../runtime/north/cc_node.h"
#include "../../../../../runtime/north/cc_msgpack_helper.h"

static int looper_callback(int fd, int events, void *data)
{
	calvinsys_t* calvinsys = (calvinsys_t*) data;
	android_sensor_data_t* data_acc = (android_sensor_data_t*) calvinsys->state;

	// write data on the output function
	if (data_acc->sensor != NULL && data_acc->queue != NULL) {
		ASensorEvent event;
		ASensorEventQueue_getEvents(data_acc->queue, &event, 1);

		char buffer[8 + mp_sizeof_float(event.acceleration.x) + mp_sizeof_float(event.acceleration.y) + mp_sizeof_float(event.acceleration.z)];
		char* w = buffer;
		w = mp_encode_map(w, 3);
		w = encode_float(&w, "x", event.acceleration.x);
		w = encode_float(&w, "y", event.acceleration.y);
		w = encode_float(&w, "z", event.acceleration.z);
		calvinsys->new_data = 1;
		calvinsys->command = COMMAND_ACCELEROMETER_DATA;
		calvinsys->data = buffer;
		calvinsys->data_size = w - buffer;
	}
}

static result_t init(calvinsys_t* calvinsys)
{
	android_sensor_data_t* data_acc = (android_sensor_data_t*) calvinsys->state;
	ASensorEventQueue_setEventRate(data_acc->queue, data_acc->sensor, (1000L/1)*10000); // (1000L/1)*1000
	ASensorEventQueue_enableSensor(data_acc->queue, data_acc->sensor);
	return CC_RESULT_SUCCESS;
}

static result_t release(calvinsys_t* calvinsys)
{
	android_sensor_data_t* data_acc = (android_sensor_data_t*) calvinsys->state;
	ASensorEventQueue_disableSensor(data_acc->queue, data_acc->sensor);
	return CC_RESULT_SUCCESS;
}

static result_t input(calvinsys_t* calvinsys, char* command, char* data, size_t data_size)
{
	// This calvinsys does not listen for input data
}

result_t create_accelerometer(node_t* node, calvinsys_t** calvinsys)
{
	log("create accelerometer");
	android_platform_t* platform;
	calvinsys_t *accelerometer_sys;
	ASensorManager* mg;
	android_sensor_data_t* data_acc;

	if (platform_mem_alloc((void **) &accelerometer_sys, sizeof(calvinsys_t)) != CC_RESULT_SUCCESS) {
		log_error("Could not allocate memory for accelerometer sys");
		return CC_RESULT_FAIL;
	}
	if (platform_mem_alloc((void **) &(accelerometer_sys->name), 32) != CC_RESULT_SUCCESS) {
		log_error("Could not allocate memory for the name of the accelerometer sys.");
		platform_mem_free(accelerometer_sys);
		return CC_RESULT_FAIL;
	}
	if (platform_mem_alloc((void **)&data_acc, sizeof(android_sensor_data_t))) {
		log_error("Could not allocate memory for the sensor data struct");
		platform_mem_free(accelerometer_sys->name);
		platform_mem_free(accelerometer_sys);
		return CC_RESULT_FAIL;
	}
	platform = (android_platform_t*) node->platform;
	mg = ASensorManager_getInstance();
	data_acc->queue = ASensorManager_createEventQueue(mg, platform->looper, 10, &looper_callback, accelerometer_sys);
	data_acc->sensor = (ASensor*) ASensorManager_getDefaultSensor(mg, ASENSOR_TYPE_ACCELEROMETER);
	if (data_acc->sensor == NULL) {
		// Sensor did not exists on this device
		log("Sensor did not exist...");
		platform_mem_free(accelerometer_sys->name);
		platform_mem_free(accelerometer_sys);
		platform_mem_free(data_acc);
		return CC_RESULT_FAIL;
	}

	strcpy(accelerometer_sys->name, "calvinsys.sensors.accelerometer");
	accelerometer_sys->write = &input;
	accelerometer_sys->init = &init;
	accelerometer_sys->release = &release;
	accelerometer_sys->state = (void*) data_acc;
	*calvinsys = accelerometer_sys;
	return CC_RESULT_SUCCESS;
}
