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
#include "cc_stepcounter.h"
#include "runtime/south/platform/cc_platform.h"
#include "runtime/south/platform/android/cc_platform_android.h"
#include "msgpuck/msgpuck.h"
#include "calvinsys/cc_calvinsys.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/coder/cc_coder.h"

static bool stepcounter_can_read(struct cc_calvinsys_obj_t *obj)
{
	android_sensor_data_t *data_acc = (android_sensor_data_t *)obj->state;
	return data_acc->data != NULL && data_acc->data_size > 0;
}

static cc_result_t stepcounter_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
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

static cc_result_t stepcounter_close(cc_calvinsys_obj_t *obj)
{
	android_sensor_data_t *data_acc = (android_sensor_data_t *)obj->state;

	ASensorEventQueue_disableSensor(data_acc->queue, data_acc->sensor);

	if (data_acc->data != NULL)
		cc_platform_mem_free((void *)data_acc->data);
	cc_platform_mem_free((void *)data_acc);

	return CC_SUCCESS;
}

static int stepcounter_looper_callback(int fd, int events, void *data)
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

		size = 50;

		if (cc_platform_mem_alloc((void **)&data_acc->data, size) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_ANDROID_LOOPER_CALLBACK_RESULT_UNREGISTER;
		}

		w = data_acc->data;
		cc_log_debug("Steps: %d", event.u64.step_counter);
		w = mp_encode_uint(w, event.u64.step_counter);
		data_acc->data_size = w - data_acc->data;

		return CC_ANDROID_LOOPER_CALLBACK_RESULT_CONTINUE;
	} else {
		cc_log_error("Could not get state of acc calvinsys");
	}
	return CC_ANDROID_LOOPER_CALLBACK_RESULT_UNREGISTER;
}

static cc_calvinsys_obj_t *stepcounter_open(cc_calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char* capability_name)
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
	data_acc->queue = ASensorManager_createEventQueue(mg, platform->looper, ALOOPER_POLL_CALLBACK, &stepcounter_looper_callback, obj);
	data_acc->sensor = get_sensor_by_name("android.sensor.step_counter");
	if (data_acc->sensor == NULL) {
		cc_log_error("Failed to get sensor");
		cc_platform_mem_free((void *)obj);
		cc_platform_mem_free((void *)data_acc);
		return NULL;
	}

	obj->write = NULL;
	obj->can_read = stepcounter_can_read;
	obj->read = stepcounter_read;
	obj->close = stepcounter_close;
	obj->handler = handler;
	obj->state = (void *)data_acc;

	ASensorEventQueue_setEventRate(data_acc->queue, data_acc->sensor, 1L);
	ASensorEventQueue_enableSensor(data_acc->queue, data_acc->sensor);

	return obj;
}

cc_result_t calvinsys_stepcounter_create(cc_calvinsys_t **calvinsys, const char *name)
{
	cc_calvinsys_handler_t *handler = NULL;

	if (cc_platform_mem_alloc((void **)&handler, sizeof(cc_calvinsys_handler_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	handler->open = stepcounter_open;
	handler->objects = NULL;
	handler->next = NULL;

	cc_calvinsys_add_handler(calvinsys, handler);
	if (cc_calvinsys_register_capability(*calvinsys, name, handler, NULL) != CC_SUCCESS)
		return CC_FAIL;

	return CC_SUCCESS;
}
