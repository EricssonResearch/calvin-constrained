/*
 * Copyright (c) 2016 Ericsson AB
 *
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

#include "calvinsys_android.h"
#include <android/log.h>
#include <android/sensor.h>
#include <string.h>
#include <unistd.h>
#include "platform.h"
#include "platform_android.h"
#include "node.h"
#include "platform_android.h"

android_sensor_data_t* data_acc;
android_sensor_data_t* data_gyro;

void get_available_sensors()
{
	ASensorManager* a = ASensorManager_getInstance();
	ASensorList list;
	int sensor_count = ASensorManager_getSensorList(a, &list);
	int i;
	for (i = 0; i < sensor_count; i++) {
		log("Sensor: %s", ASensor_getName(list[i]));
	}
	const ASensor* acc = ASensorManager_getDefaultSensor(a, ASENSOR_TYPE_ACCELEROMETER);
	if (acc != NULL) {

	}
}

static result_t init_enviromental()
{

}

static result_t get_temperature(double *temp)
{
	//*temp = (double) data_acc->event->acceleration.x;
	*temp = 87;
	return SUCCESS;
}

static result_t get_humidity(double* humidity)
{
	*humidity = 1993;
	return SUCCESS;
}

static result_t get_pressure(double* pressure)
{
	*pressure = 1995;
	return SUCCESS;
}

static result_t platform_create_sensors_environmental(node_t *node)
{
	char name[] = "calvinsys.sensors.environmental";
	calvinsys_sensors_environmental_t *sensors_env = NULL;

	if (platform_mem_alloc((void **)&sensors_env, sizeof(calvinsys_sensors_environmental_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	sensors_env->init_enviromental = init_enviromental;
	sensors_env->get_temperature = get_temperature;
	sensors_env->get_humidity = get_humidity;
	sensors_env->get_pressure = get_pressure;
	return list_add_n(&node->calvinsys, name, strlen(name)+1, sensors_env, sizeof(calvinsys_sensors_environmental_t));
}

static int set_event(int fd, int events, void *data)
{
	android_sensor_data_t* sensor_data = (android_sensor_data_t*) data;
	if (sensor_data->sensor != NULL && sensor_data->queue != NULL) {
		if (sensor_data->event != NULL) {
			platform_mem_free(sensor_data->event);
		}
		ASensorEvent* event;
		platform_mem_alloc((void**) &event, sizeof(ASensorEvent));
		ASensorEventQueue_getEvents(sensor_data->queue, event, 1);
	}
}

result_t create_calvinsys(node_t* node)
{
	android_platform_t* platform;

	platform = (android_platform_t*) node->platform;
	platform->looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

	log("Create calvinsys");
	platform_create_sensors_environmental(node);

	ASensorManager* mg;

	mg = ASensorManager_getInstance();

	platform_mem_alloc((void **)&data_acc, sizeof(android_sensor_data_t));
	data_acc->queue = ASensorManager_createEventQueue(mg, platform->looper, 10, &set_event, data_acc);
	data_acc->sensor = (ASensor*) ASensorManager_getDefaultSensor(mg, ASENSOR_TYPE_ACCELEROMETER);
	ASensorEventQueue_setEventRate(data_acc->queue, data_acc->sensor, (1000L/1)*1000);
	ASensorEventQueue_enableSensor(data_acc->queue, data_acc->sensor);

	platform_mem_alloc((void **)&data_gyro, sizeof(android_sensor_data_t));
	data_gyro->queue = ASensorManager_createEventQueue(mg, platform->looper, 10, &set_event, data_gyro);
	data_gyro->sensor = (ASensor*) ASensorManager_getDefaultSensor(mg, ASENSOR_TYPE_GYROSCOPE);
	ASensorEventQueue_setEventRate(data_gyro->queue, data_gyro->sensor, (1000L/1)*1000);
	ASensorEventQueue_enableSensor(data_gyro->queue, data_gyro->sensor);
	return SUCCESS;
}