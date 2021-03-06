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
#include "runtime/south/platform/cc_platform.h"
#include "runtime/south/platform/android/cc_platform_android.h"
#include "calvinsys/cc_calvinsys.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/coder/cc_coder.h"

static bool cc_accelerometer_can_read(struct cc_calvinsys_obj_t *obj)
{
	cc_android_sensor_data_t *data_acc = (cc_android_sensor_data_t *)obj->state;

	return data_acc->data != NULL && data_acc->data_size > 0;
}

static cc_result_t cc_accelerometer_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	cc_android_sensor_data_t *data_acc = (cc_android_sensor_data_t *)obj->state;

	if (data_acc->data != NULL && data_acc->data_size > 0) {
		*data = data_acc->data;
		*size = data_acc->data_size;
		data_acc->data = NULL;
		data_acc->data_size = 0;
		return CC_SUCCESS;
	}

	return CC_FAIL;
}

static cc_result_t cc_accelerometer_close(cc_calvinsys_obj_t *obj)
{
	cc_android_sensor_data_t *data_acc = (cc_android_sensor_data_t *)obj->state;

	ASensorEventQueue_disableSensor(data_acc->queue, data_acc->sensor);
	if (data_acc->data != NULL)
		cc_platform_mem_free((void *)data_acc->data);
	cc_platform_mem_free((void *)data_acc);

	return CC_SUCCESS;
}

static int cc_accelerometer_looper_callback(int fd, int events, void *data)
{
	cc_calvinsys_obj_t *obj = (cc_calvinsys_obj_t *)data;
	cc_android_sensor_data_t *data_acc = (cc_android_sensor_data_t *)obj->state;
	size_t size = 0;
	char *w = NULL;

	// write data on the output function
	if (data_acc->sensor != NULL && data_acc->queue != NULL) {
		ASensorEvent event;
		if (ASensorEventQueue_getEvents(data_acc->queue, &event, 1) < 0){
			cc_log_error("Failed to get events from sensor.");
			return CC_ANDROID_LOOPER_CALLBACK_RESULT_CONTINUE;
		}

		if (data_acc->data != NULL)
			cc_platform_mem_free((void *)data_acc->data);

		size = 50 + cc_coder_sizeof_float(event.acceleration.x) + cc_coder_sizeof_float(event.acceleration.y) + cc_coder_sizeof_float(event.acceleration.z);

		if (cc_platform_mem_alloc((void **)&data_acc->data, size) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_ANDROID_LOOPER_CALLBACK_RESULT_UNREGISTER;
		}

		w = data_acc->data;
		w = cc_coder_encode_map(w, 3);
		w = cc_coder_encode_kv_float(w, "x", event.acceleration.x);
		w = cc_coder_encode_kv_float(w, "y", event.acceleration.y);
		w = cc_coder_encode_kv_float(w, "z", event.acceleration.z);
		data_acc->data_size = w - data_acc->data;
		return CC_ANDROID_LOOPER_CALLBACK_RESULT_CONTINUE;
	} else {
		cc_log_error("Could not get state of acc calvinsys");
	}
	return CC_ANDROID_LOOPER_CALLBACK_RESULT_UNREGISTER;
}

static char *cc_accelerometer_serialize(char *id, cc_calvinsys_obj_t *obj, char *buffer)
{
	cc_android_sensor_data_t *data_acc = (cc_android_sensor_data_t *)obj->state;

	buffer = cc_coder_encode_kv_map(buffer, "obj", 1);
	{
		buffer = cc_coder_encode_kv_uint(buffer, "period", data_acc->period);
	}

	return buffer;
}

cc_result_t cc_accelerometer_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	cc_android_sensor_data_t *data_acc = NULL;
	cc_platform_android_t *platform = (cc_platform_android_t *)obj->capability->calvinsys->node->platform;
	ASensorManager *mg = NULL;
	cc_list_t *item = NULL;
	uint32_t period = 0;

	item = cc_list_get(kwargs, "period");
	if (item == NULL) {
		cc_log_error("Failed to get 'period'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint(item->data, &period) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'period'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&data_acc, sizeof(cc_android_sensor_data_t))) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	mg = ASensorManager_getInstance();
	data_acc->data = NULL;
	data_acc->queue = ASensorManager_createEventQueue(mg, platform->looper, ALOOPER_POLL_CALLBACK, &cc_accelerometer_looper_callback, obj);
	data_acc->sensor = (ASensor *)ASensorManager_getDefaultSensor(mg, ASENSOR_TYPE_ACCELEROMETER);
	if (data_acc->sensor == NULL) {
		cc_log_error("Failed to get sensor");
		cc_platform_mem_free((void *)data_acc);
		return CC_FAIL;
	}
	data_acc->period = period;

	obj->can_read = cc_accelerometer_can_read;
	obj->read = cc_accelerometer_read;
	obj->close = cc_accelerometer_close;
	obj->state = (void *)data_acc;
	obj->serialize = cc_accelerometer_serialize;

	ASensorEventQueue_setEventRate(data_acc->queue, data_acc->sensor, period);
	ASensorEventQueue_enableSensor(data_acc->queue, data_acc->sensor);

	cc_log("Opened accelerometer with period %ld", period);

	return CC_SUCCESS;
}
