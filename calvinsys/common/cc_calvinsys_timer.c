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
#include "cc_calvinsys_timer.h"
#include "runtime/north/cc_common.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/coder/cc_coder.h"

static void cc_calvinsys_timer_set(cc_node_t *node, cc_calvinsys_timer_t *timer, uint32_t timeout)
{
	timer->armed = true;
	timer->next_time = cc_node_get_time(node) + timeout;
}

static bool cc_calvinsys_timer_can_read(struct cc_calvinsys_obj_t *obj)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;

	return timer->triggered;
}

static cc_result_t cc_calvinsys_timer_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;

	timer->triggered = false;
	if (timer->repeats)
		cc_calvinsys_timer_set(obj->capability->calvinsys->node, timer, timer->timeout);

	return CC_SUCCESS;
}

static bool cc_calvinsys_timer_can_write(struct cc_calvinsys_obj_t *obj)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;

	return !(timer->triggered || timer->armed);
}

static cc_result_t cc_calvinsys_timer_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;
	uint32_t timeout = 0;

	if (!cc_calvinsys_timer_can_write(obj))
		return CC_FAIL;

	if (cc_coder_decode_uint(data, &timeout) != CC_SUCCESS) {
		cc_log_error("Failed to decode value as uint");
		return CC_FAIL;
	}

	timer->timeout = timeout;
	cc_calvinsys_timer_set(obj->capability->calvinsys->node, timer, timer->timeout);

	return CC_SUCCESS;
}

static cc_result_t cc_calvinsys_timer_close(struct cc_calvinsys_obj_t *obj)
{
	cc_log("Timer '%s' closed.", obj->id);
	cc_platform_mem_free((void *)obj->state);
	return CC_SUCCESS;
}

static char *cc_calvinsys_timer_serialize(char *id, cc_calvinsys_obj_t *obj, char *buffer)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;

	buffer = cc_coder_encode_kv_map(buffer, "obj", 4);
	{
		if (timer->armed)
			buffer = cc_coder_encode_kv_uint(buffer, "nexttrigger", timer->next_time);
		else
			buffer = cc_coder_encode_kv_nil(buffer, "nexttrigger");
		buffer = cc_coder_encode_kv_bool(buffer, "repeats", timer->repeats);
		buffer = cc_coder_encode_kv_uint(buffer, "timeout", timer->timeout);
		buffer = cc_coder_encode_kv_bool(buffer, "triggered", timer->triggered);
	}

	return buffer;
}

static cc_result_t cc_calvinsys_timer_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	cc_calvinsys_timer_t *timer = NULL;
	cc_list_t *item = NULL;
	uint32_t period = 0;

	if (cc_platform_mem_alloc((void **)&timer, sizeof(cc_calvinsys_timer_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	obj->can_write = cc_calvinsys_timer_can_write;
	obj->write = cc_calvinsys_timer_write;
	obj->can_read = cc_calvinsys_timer_can_read;
	obj->read = cc_calvinsys_timer_read;
	obj->close = cc_calvinsys_timer_close;
	obj->serialize = cc_calvinsys_timer_serialize;
	obj->state = timer;

	item = cc_list_get(kwargs, "period");
	if (item != NULL) {
		if (cc_coder_decode_uint(item->data, &period) != CC_SUCCESS) {
			cc_log_error("Failed to decode 'period'");
			return CC_FAIL;
		}
	}

	timer->timeout = period;
	timer->armed = false;
	timer->triggered = false;
	if (strncmp(obj->capability->name, "sys.timer.repeating", 19)  == 0)
		timer->repeats = true;
	else
		timer->repeats = false;

	if (period > 0)
		cc_calvinsys_timer_set(obj->capability->calvinsys->node, timer, timer->timeout);

	cc_log("Timer '%s' created, armed %d timeout '%ld'", obj->id, timer->armed, timer->timeout);

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_timer_deserialize(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	cc_calvinsys_timer_t *timer = NULL;
	uint32_t timeout = 0, nexttrigger = 0;
	bool repeats = false, triggered = false;
	cc_coder_type_t type = CC_CODER_UNDEF;
	cc_list_t *item = NULL;

	item = cc_list_get(kwargs, "timeout");
	if (item == NULL) {
		cc_log_error("Failed to get 'timeout'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint(item->data, &timeout) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'timeout'");
		return CC_FAIL;
	}

	item = cc_list_get(kwargs, "repeats");
	if (item == NULL) {
		cc_log_error("Failed to get 'repeats'");
		return CC_FAIL;
	}

	if (cc_coder_decode_bool(item->data, &repeats) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'repeats'");
		return CC_FAIL;
	}

	item = cc_list_get(kwargs, "triggered");
	if (item == NULL) {
		cc_log_error("Failed to get 'triggered'");
		return CC_FAIL;
	}

	if (cc_coder_decode_bool(item->data, &triggered) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'triggered'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&timer, sizeof(cc_calvinsys_timer_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(timer, 0, sizeof(cc_calvinsys_timer_t));
	timer->triggered = triggered;
	timer->timeout = timeout;
	timer->repeats = repeats;

	item = cc_list_get(kwargs, "nexttrigger");
	if (item != NULL) {
		type = cc_coder_type_of(item->data);
		if (type == CC_CODER_UNDEF || type == CC_CODER_NIL)
			nexttrigger = 0;
		else if (type == CC_CODER_FLOAT || type == CC_CODER_DOUBLE || type == CC_CODER_UINT)
			cc_coder_decode_uint(item->data, &nexttrigger);
		timeout = nexttrigger - cc_node_get_time(obj->capability->calvinsys->node);
		cc_calvinsys_timer_set(obj->capability->calvinsys->node, timer, timeout);
	} else {
		timer->armed = false;
	}

	obj->can_write = cc_calvinsys_timer_can_write;
	obj->write = cc_calvinsys_timer_write;
	obj->can_read = cc_calvinsys_timer_can_read;
	obj->read = cc_calvinsys_timer_read;
	obj->close = cc_calvinsys_timer_close;
	obj->serialize = cc_calvinsys_timer_serialize;
	obj->state = timer;

	cc_log("Timer '%s' deserialized, armed '%d' timeout '%ld'", obj->id, timer->armed, timer->timeout);

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_timer_create(cc_calvinsys_t **calvinsys)
{
	if (cc_calvinsys_create_capability(*calvinsys, "sys.timer.once",
			cc_calvinsys_timer_open,
			cc_calvinsys_timer_deserialize,
			NULL,
			false) != CC_SUCCESS) {
		cc_log_error("Failed to create 'sys.timer.once'");
	 	return CC_FAIL;
	}

	if (cc_calvinsys_create_capability(*calvinsys, "sys.timer.repeating",
			cc_calvinsys_timer_open,
			cc_calvinsys_timer_deserialize,
			NULL,
			false) != CC_SUCCESS) {
		cc_log_error("Failed to create 'sys.timer.repeating'");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

void cc_calvinsys_timers_check(cc_node_t *node, uint32_t *timeout)
{
	cc_list_t *list = node->calvinsys->objects;
	cc_calvinsys_timer_t *timer = NULL;
	cc_calvinsys_obj_t *obj = NULL;
	uint32_t now = cc_node_get_time(node);

	while (list != NULL) {
		obj = (cc_calvinsys_obj_t *)list->data;
		if (strncmp(obj->capability->name, "sys.timer", 9) == 0) {
			timer = (cc_calvinsys_timer_t *)obj->state;
			if (timer->armed) {
				if (now >= timer->next_time) {
					timer->triggered = true;
					timer->armed = false;
					*timeout = 0;
				} else {
					*timeout = timer->next_time - now;
				}
			}
		}
		list = list->next;
	}
}
