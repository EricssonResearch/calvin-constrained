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
#include "cc_calvinsys_timer.h"
#include "../../runtime/north/cc_common.h"
#include "../../runtime/north/cc_node.h"
#include "../../runtime/north/cc_msgpack_helper.h"
#include "../cc_calvinsys.h"
#include "../../msgpuck/msgpuck.h"

static bool calvinsys_timer_can_read(struct calvinsys_obj_t *obj)
{
	calvinsys_timer_t *timer = (calvinsys_timer_t *)obj->state;
	uint32_t now = platform_get_seconds(obj->handler->calvinsys->node);

  if (timer->active && (now - timer->last_triggered) >= timer->timeout)
		return true;

	return false;
}

static result_t calvinsys_timer_read(struct calvinsys_obj_t *obj, char **data, size_t *size)
{
	calvinsys_timer_t *timer = (calvinsys_timer_t *)obj->state;

	timer->active = false;
	timer->last_triggered = 0;

	return CC_RESULT_SUCCESS;
}

static bool calvinsys_timer_can_write(struct calvinsys_obj_t *obj)
{
	return true;
}

static result_t calvinsys_timer_write(struct calvinsys_obj_t *obj, char *data, size_t size)
{
	calvinsys_timer_t *timer = (calvinsys_timer_t *)obj->state;
	uint32_t uint_value = 0;
	bool bool_value = false;

	switch (mp_typeof(*data)) {
		case MP_UINT:
		if (decode_uint(data, &uint_value) == CC_RESULT_SUCCESS) {
			timer->last_triggered = platform_get_seconds(obj->handler->calvinsys->node);
			timer->active = true;
			timer->timeout = uint_value;
			return CC_RESULT_SUCCESS;
		}
		break;
		case MP_BOOL:
		if (decode_bool(data, &bool_value) == CC_RESULT_SUCCESS) {
			if (bool_value) {
				timer->last_triggered = platform_get_seconds(obj->handler->calvinsys->node);
				timer->active = true;
			} else {
				timer->last_triggered = 0;
				timer->active = false;
			}
			return CC_RESULT_SUCCESS;
		}
		break;
		default:
			cc_log_error("Unknown type");
	}

	return CC_RESULT_FAIL;
}


static result_t calvinsys_timer_close(struct calvinsys_obj_t *obj)
{
	platform_mem_free((void *)obj->state);
  obj->handler->objects = NULL; // TODO: Support multiple timers
	return CC_RESULT_SUCCESS;
}

static calvinsys_obj_t *calvinsys_timer_open(calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char *capability_name)
{
	calvinsys_obj_t *obj = NULL;
	calvinsys_timer_t *timer = NULL;

	if (platform_mem_alloc((void **)&timer, sizeof(calvinsys_timer_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	if (platform_mem_alloc((void **)&obj, sizeof(calvinsys_obj_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		platform_mem_free((void *)timer);
		return NULL;
	}

	timer->timeout = 0;
	timer->last_triggered = 0;
	timer->active = false;

	if (decode_uint_from_map(data, "timeout", &timer->timeout) == CC_RESULT_SUCCESS) {
		if (timer->timeout > 0) {
			timer->active = true;
			decode_uint_from_map(data, "last_triggered", &timer->last_triggered);
		}
	}

	cc_log("Timer created with timeout '%ld' s last triggered: '%ld'", timer->timeout, timer->last_triggered);

	obj->can_write = calvinsys_timer_can_write;
	obj->write = calvinsys_timer_write;
	obj->can_read = calvinsys_timer_can_read;
	obj->read = calvinsys_timer_read;
	obj->close = calvinsys_timer_close;
	obj->state = timer;

	return obj;
}

result_t calvinsys_timer_create(calvinsys_t **calvinsys)
{
	calvinsys_handler_t *handler = NULL;

	if (platform_mem_alloc((void **)&handler, sizeof(calvinsys_handler_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	handler->open = calvinsys_timer_open;
	handler->objects = NULL;
	handler->next = NULL;

	calvinsys_add_handler(calvinsys, handler);
	if (calvinsys_register_capability(*calvinsys, "sys.timer.once", handler, NULL) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	return CC_RESULT_SUCCESS;
}

result_t calvinsys_timer_get_next_timeout(node_t *node, uint32_t *timeout)
{
	uint32_t now = 0, diff = 0;
	calvinsys_capability_t *timer_capability = (calvinsys_capability_t *)list_get(node->calvinsys->capabilities, "sys.timer.once");
	calvinsys_obj_t *timer_obj = NULL;
	calvinsys_timer_t *state = NULL;
	bool first = true;
	result_t result = CC_RESULT_FAIL;

	if (timer_capability != NULL) {
		timer_obj = timer_capability->handler->objects;
		*timeout = 0;
		now = platform_get_seconds(node);
		timer_obj = timer_capability->handler->objects;
		while (timer_obj != NULL) {
			state = (calvinsys_timer_t *)timer_obj->state;
			if (state->active) {
				diff = ((calvinsys_timer_t *)timer_obj->state)->timeout - (now - ((calvinsys_timer_t *)timer_obj->state)->last_triggered);
				if (first) {
					*timeout = diff;
					first = false;
				} else {
					if (diff < *timeout)
						*timeout = diff;
				}
				result = CC_RESULT_SUCCESS;
			}
			timer_obj = timer_obj->next;
		}
	}

	return result;
}
