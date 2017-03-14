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

ASensorManager* mg;

android_sensor_data_t* data_acc;
android_sensor_data_t* data_gyro;
android_sensor_data_t* data_proximity;

static result_t init_enviromental()
{

}

static result_t get_temperature(double *temp)
{
	*temp = 15;
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

// ====================================
// ==== SENSOR_ACCELEROMETER START ====
// ====================================
static result_t activate_accelerometer(node_t* node, long event_rate)
{
	ASensorEventQueue_setEventRate(data_acc->queue, data_acc->sensor, event_rate); // (1000L/1)*1000
	ASensorEventQueue_enableSensor(data_acc->queue, data_acc->sensor);
	return SUCCESS;
}

static result_t dectivate_accelerometer()
{
	ASensorEventQueue_disableSensor(data_acc->queue, data_acc->sensor);
	return SUCCESS;
}

static result_t platform_accelerometer_get_acceleration(int* acceleration)
{
	if (data_acc->event != NULL) {
		acceleration[0] = data_acc->event->acceleration.x;
		acceleration[1] = data_acc->event->acceleration.y;
		acceleration[2] = data_acc->event->acceleration.z;
		return SUCCESS;
	} else {
		return FAIL;
	}
}

static result_t platform_create_sensor_accelerometer(node_t* node)
{
	android_platform_t* platform;
	platform = (android_platform_t*) node->platform;
	platform_mem_alloc((void **)&data_acc, sizeof(android_sensor_data_t));
	data_acc->queue = ASensorManager_createEventQueue(mg, platform->looper, 10, &set_event, data_acc);
	data_acc->sensor = (ASensor*) ASensorManager_getDefaultSensor(mg, ASENSOR_TYPE_ACCELEROMETER);
	if (data_acc->sensor == NULL) {
		// Sensor did not exists on this device
		return FAIL;
	}

	char name[] = "calvinsys.sensors.accelerometer";
	calvinsys_sensors_accelerometer_t* sensors_acc = NULL;
	if (platform_mem_alloc((void **)&sensors_acc, sizeof(calvinsys_sensors_accelerometer_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}
	sensors_acc->activate = activate_accelerometer;
	sensors_acc->deactivate = dectivate_accelerometer;
	sensors_acc->get_acceleration = platform_accelerometer_get_acceleration;
	return list_add_n(&node->calvinsys, name, strlen(name)+1, sensors_acc, sizeof(calvinsys_sensors_accelerometer_t));
}
// ====================================
// ===== SENSOR_ACCELEROMETER END =====
// ====================================

// ====================================
// ======== SENSOR_GYRO START =========
// ====================================
static result_t activate_gyroscope(node_t* node, long event_rate)
{
	ASensorEventQueue_setEventRate(data_gyro->queue, data_gyro->sensor, event_rate); // (1000L/1)*1000
	ASensorEventQueue_enableSensor(data_gyro->queue, data_gyro->sensor);
	return SUCCESS;
}

static result_t dectivate_gyroscope()
{
	ASensorEventQueue_disableSensor(data_gyro->queue, data_gyro->sensor);
	return SUCCESS;
}

static result_t platform_gyroscope_get_orientation(int* orientation)
{
	if (data_gyro->event != NULL) {
		orientation[0] = data_gyro->event->vector.x;
		orientation[1] = data_gyro->event->vector.y;
		orientation[2] = data_gyro->event->vector.z;
	} else {
		return FAIL;
	}
}

static result_t platform_create_sensor_gyroscope(node_t* node)
{
	android_platform_t* platform;

	platform = (android_platform_t*) node->platform;
	platform_mem_alloc((void **)&data_gyro, sizeof(android_sensor_data_t));
	data_gyro->queue = ASensorManager_createEventQueue(mg, platform->looper, 10, &set_event, data_gyro);
	data_gyro->sensor = (ASensor*) ASensorManager_getDefaultSensor(mg, ASENSOR_TYPE_GYROSCOPE);
	if (data_gyro->sensor == NULL) {
		// Sensor did not exist on this device
		return FAIL;
	}

	char name[] = "calvinsys.sensors.gyroscope";
	calvinsys_sensors_gyroscope_t* sensors_gyro = NULL;
	if (platform_mem_alloc((void **)&sensors_gyro, sizeof(calvinsys_sensors_gyroscope_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}
	sensors_gyro->activate = activate_gyroscope;
	sensors_gyro->deactivate = dectivate_gyroscope;
	sensors_gyro->get_orientation = platform_gyroscope_get_orientation;
	return list_add_n(&node->calvinsys, name, strlen(name)+1, sensors_gyro, sizeof(calvinsys_sensors_gyroscope_t));
}
// ====================================
// ========= SENSOR_GYRO END ==========
// ====================================

result_t create_calvinsys(node_t* node)
{
	android_platform_t* platform;

	platform = (android_platform_t*) node->platform;
	mg = ASensorManager_getInstance();
	platform_create_sensors_environmental(node);
	platform_create_sensor_accelerometer(node);
	platform_create_sensor_gyroscope(node);
	return SUCCESS;
}
