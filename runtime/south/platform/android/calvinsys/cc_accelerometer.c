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
#include "cc_accelerometer.h"
#include "../../../../../runtime/south/platform/cc_platform.h"
#include "../../../../../runtime/south/platform/android/cc_platform_android.h"
#include "../../../../../calvinsys/cc_calvinsys.h"
#include "../../../../../runtime/north/cc_node.h"
#include "../../../../../runtime/north/coder/cc_coder.h"

static bool accelerometer_can_read(struct cc_calvinsys_obj_t *obj)
{
	android_sensor_data_t *data_acc = (android_sensor_data_t *)obj->state;
	return data_acc->data != NULL && data_acc->data_size > 0;
}

static cc_result_t accelerometer_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	android_sensor_data_t *data_acc = (android_sensor_data_t *)obj->state;

	if (data_acc->data != NULL && data_acc->data_size > 0) {
		*data = data_acc->data;
		*size = data_acc->data_size;
		data_acc->data = NULL;
		data_acc->data_size = 0;
		return CC_SUCCESS;
	}
	return CC_FAIL;
}

static cc_result_t accelerometer_close(cc_calvinsys_obj_t *obj)
{
	android_sensor_data_t *data_acc = (android_sensor_data_t *)obj->state;

	ASensorEventQueue_disableSensor(data_acc->queue, data_acc->sensor);

	if (data_acc->data != NULL)
		cc_platform_mem_free((void *)data_acc->data);
	cc_platform_mem_free((void *)data_acc);

	return CC_SUCCESS;
}

static int accelerometer_looper_callback(int fd, int events, void *data)
{
	cc_calvinsys_obj_t *obj = (cc_calvinsys_obj_t *)data;
	android_sensor_data_t *data_acc = (android_sensor_data_t *)obj->state;
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

static cc_calvinsys_obj_t *accelerometer_open(cc_calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char* capability_name)
{
	cc_calvinsys_obj_t *obj = NULL;

	android_sensor_data_t *data_acc = NULL;
	android_platform_t* platform = (android_platform_t*) handler->calvinsys->node->platform;

	ASensorManager *mg= ASensorManager_getInstance();
	if (cc_platform_mem_alloc((void **)&obj, sizeof(cc_calvinsys_obj_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	if (cc_platform_mem_alloc((void **)&data_acc, sizeof(android_sensor_data_t))) {
		cc_log_error("Failed to allocate memory");
		cc_platform_mem_free((void *)obj);
		return NULL;
	}

	data_acc->data = NULL;
	data_acc->queue = ASensorManager_createEventQueue(mg, platform->looper, ALOOPER_POLL_CALLBACK, &accelerometer_looper_callback, obj);
	data_acc->sensor = (ASensor *) ASensorManager_getDefaultSensor(mg, ASENSOR_TYPE_ACCELEROMETER);
	if (data_acc->sensor == NULL) {
		cc_log_error("Failed to get sensor");
		cc_platform_mem_free((void *)obj);
		cc_platform_mem_free((void *)data_acc);
		return NULL;
	}

	obj->write = NULL;
	obj->can_read = accelerometer_can_read;
	obj->read = accelerometer_read;
	obj->close = accelerometer_close;
	obj->handler = handler;
	obj->state = (void *)data_acc;

	ASensorEventQueue_setEventRate(data_acc->queue, data_acc->sensor, 1L);
	ASensorEventQueue_enableSensor(data_acc->queue, data_acc->sensor);

	return obj;
}

cc_result_t calvinsys_accelerometer_create(cc_calvinsys_t **calvinsys, const char *name)
{
	cc_calvinsys_handler_t *handler = NULL;

	if (cc_platform_mem_alloc((void **)&handler, sizeof(cc_calvinsys_handler_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	handler->open = accelerometer_open;
	handler->objects = NULL;
	handler->next = NULL;

	cc_calvinsys_add_handler(calvinsys, handler);
	if (cc_calvinsys_register_capability(*calvinsys, name, handler, NULL) != CC_SUCCESS)
		return CC_FAIL;

	return CC_SUCCESS;
}
