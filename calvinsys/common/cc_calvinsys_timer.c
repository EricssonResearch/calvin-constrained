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
#include "../../runtime/north/cc_common.h"
#include "../../runtime/north/cc_node.h"
#include "../../runtime/north/coder/cc_coder.h"
#include "../cc_calvinsys.h"

static bool cc_calvinsys_timer_can_read(struct cc_calvinsys_obj_t *obj)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;
	uint32_t now = cc_node_get_time(obj->capability->calvinsys->node);

	if (timer->active && now >= timer->nexttrigger)
		return true;

	return false;
}

static cc_result_t cc_calvinsys_timer_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;
	uint32_t now = 0;

	if (timer->repeats) {
		now = cc_node_get_time(obj->capability->calvinsys->node);
		timer->nexttrigger = cc_node_get_time(obj->capability->calvinsys->node) + timer->timeout;
		cc_log_debug("Timer '%s' set, now '%ld' nexttrigger '%ld'", obj->id, now, timer->nexttrigger);
	} else {
		cc_log_debug("Timer '%s' cleared", obj->id);
		timer->active = false;
	}

	return CC_SUCCESS;
}

static bool cc_calvinsys_timer_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t cc_calvinsys_timer_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;
	cc_node_t *node = obj->capability->calvinsys->node;
	uint32_t now = 0, timeout = 0;

	if (cc_coder_type_of(data) == CC_CODER_BOOL) {
		bool value = false;
		if (cc_coder_decode_bool(data, &value) != CC_SUCCESS) {
			cc_log_error("Failed to decode value as bool");
			return CC_FAIL;
		}
		if (value)
			timeout = timer->timeout;
	} else {
		if (cc_coder_decode_uint(data, &timeout) != CC_SUCCESS) {
			cc_log_error("Failed to decode value as uint");
			return CC_FAIL;
		}
	}

	if (timeout > 0) {
		now = cc_node_get_time(node);
		timer->timeout = timeout;
		timer->nexttrigger = now + timeout;
		timer->active = true;
		cc_log_debug("Timer '%s' set, now '%ld' nexttrigger '%ld'", obj->id, now, timer->nexttrigger);
	} else
		timer->active = false;

	return CC_SUCCESS;
}

static cc_result_t cc_calvinsys_timer_close(struct cc_calvinsys_obj_t *obj)
{
	cc_platform_mem_free((void *)obj->state);
	return CC_SUCCESS;
}

static char *cc_calvinsys_timer_serialize(char *id, cc_calvinsys_obj_t *obj, char *buffer)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;

	buffer = cc_coder_encode_kv_map(buffer, "obj", 3);
	{
		buffer = cc_coder_encode_kv_uint(buffer, "nexttrigger", timer->nexttrigger);
		buffer = cc_coder_encode_kv_bool(buffer, "repeats", timer->repeats);
		buffer = cc_coder_encode_kv_uint(buffer, "timeout", timer->timeout);
	}

	return buffer;
}

static cc_result_t cc_calvinsys_timer_open(cc_calvinsys_obj_t *obj, char *data, size_t len)
{
	cc_calvinsys_timer_t *timer = NULL;
	uint32_t timeout = 0;

	if (data != NULL) {
		if (cc_coder_decode_uint(data, &timeout) != CC_SUCCESS) {
			cc_log_error("Failed to get 'timeout'");
			return CC_FAIL;
		}
	}

	if (cc_platform_mem_alloc((void **)&timer, sizeof(cc_calvinsys_timer_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	timer->timeout = timeout;
	if (timer->timeout != 0) {
		timer->nexttrigger = cc_node_get_time(obj->capability->calvinsys->node);
		timer->active = true;
	} else {
		timer->nexttrigger = 0;
		timer->active = false;
	}

	if (strncmp(obj->capability->name, "sys.timer.repeating", 19)  == 0)
		timer->repeats = true;
	else
		timer->repeats = false;

	obj->can_write = cc_calvinsys_timer_can_write;
	obj->write = cc_calvinsys_timer_write;
	obj->can_read = cc_calvinsys_timer_can_read;
	obj->read = cc_calvinsys_timer_read;
	obj->close = cc_calvinsys_timer_close;
	obj->serialize = cc_calvinsys_timer_serialize;
	obj->state = timer;

	cc_log("Timer '%s' created, active %d timeout '%ld'", obj->id, timer->active, timer->timeout);

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_timer_deserialize(cc_calvinsys_obj_t *obj, char *buffer)
{
	cc_calvinsys_timer_t *timer = NULL;
	uint32_t timeout = 0, nexttrigger = 0;
	bool repeats = false;
	char *value = NULL;
	cc_coder_type_t type = CC_CODER_UNDEF;

	if (cc_coder_get_value_from_map(buffer, "timeout", &value) != CC_SUCCESS) {
		cc_log_error("Failed to get 'timeout'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint(value, &timeout) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'timeout'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(buffer, "nexttrigger", &value) != CC_SUCCESS) {
		cc_log_error("Failed to get 'nexttrigger'");
		return CC_FAIL;
	}

	type = cc_coder_type_of(value);
	if (type == CC_CODER_UNDEF || type == CC_CODER_NIL)
		nexttrigger = 0;
	else if (type == CC_CODER_FLOAT || type == CC_CODER_DOUBLE || type == CC_CODER_UINT)
		cc_coder_decode_uint(value, &nexttrigger);

	if (cc_coder_decode_bool_from_map(buffer, "repeats", &repeats) != CC_SUCCESS) {
		cc_log_error("Failed to get attribute 'repeats'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&timer, sizeof(cc_calvinsys_timer_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(timer, 0, sizeof(cc_calvinsys_timer_t));
	timer->timeout = timeout;
	timer->nexttrigger = nexttrigger;
	timer->repeats = repeats;
	if (timer->timeout > 0)
		timer->active = true;
	else
		timer->active = false;

	obj->can_write = cc_calvinsys_timer_can_write;
	obj->write = cc_calvinsys_timer_write;
	obj->can_read = cc_calvinsys_timer_can_read;
	obj->read = cc_calvinsys_timer_read;
	obj->close = cc_calvinsys_timer_close;
	obj->serialize = cc_calvinsys_timer_serialize;
	obj->state = timer;

	cc_log("Timer '%s' deserialized, active '%d' timeout '%ld'", obj->id, timer->active, timer->timeout);

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_timer_create(cc_calvinsys_t **calvinsys)
{
	if (cc_calvinsys_create_capability(*calvinsys, "sys.timer.once",
			cc_calvinsys_timer_open,
			cc_calvinsys_timer_deserialize,
			NULL) != CC_SUCCESS) {
		cc_log_error("Failed to create 'sys.timer.once'");
	 	return CC_FAIL;
	}

	if (cc_calvinsys_create_capability(*calvinsys, "sys.timer.repeating",
			cc_calvinsys_timer_open,
			cc_calvinsys_timer_deserialize,
			NULL) != CC_SUCCESS) {
		cc_log_error("Failed to create 'sys.timer.repeating'");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_timer_get_nexttrigger(cc_node_t *node, uint32_t *nexttrigger)
{
	cc_list_t *list = node->calvinsys->objects;
	cc_calvinsys_timer_t *timer = NULL;
	cc_calvinsys_obj_t *obj = NULL;
	uint32_t min_nexttrigger = 0, now = 0;
	bool anyactive = false;

	while (list != NULL) {
		obj = (cc_calvinsys_obj_t *)list->data;
		if (strncmp(obj->capability->name, "sys.timer", 9) == 0) {
			timer = (cc_calvinsys_timer_t *)obj->state;
			if (!timer->active)
				continue;

			if (!anyactive) {
				min_nexttrigger = timer->nexttrigger;
				anyactive = true;
				continue;
			}

			if (timer->nexttrigger < min_nexttrigger)
				min_nexttrigger = timer->nexttrigger;
		}
		list = list->next;
	}

	if (!anyactive)
	 	return CC_FAIL;

	now = cc_node_get_time(node);
	if (min_nexttrigger < now)
		*nexttrigger = 0;
	else
		*nexttrigger = min_nexttrigger - now;

	cc_log_debug("Next trigger: %ld now: %ld", *nexttrigger, now);

	return CC_SUCCESS;
}
