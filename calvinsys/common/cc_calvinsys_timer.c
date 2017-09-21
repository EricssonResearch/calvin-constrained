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
#include "../../runtime/north/coder/cc_coder.h"
#include "../cc_calvinsys.h"

static bool calvinsys_timer_can_read(struct cc_calvinsys_obj_t *obj)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;
	uint32_t now = cc_platform_get_seconds(obj->handler->calvinsys->node);

	if (timer->active && (timer->timeout == 0 ||
		timer->last_triggered == 0 ||
		(now - timer->last_triggered) >= timer->timeout)) {
		return true;
	}

	return false;
}

static cc_result_t calvinsys_timer_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;

	timer->active = false;
	timer->last_triggered = 0;

	return CC_SUCCESS;
}

static bool calvinsys_timer_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t calvinsys_timer_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	cc_calvinsys_timer_t *timer = (cc_calvinsys_timer_t *)obj->state;

	switch (cc_coder_type_of(data)) {
		case CC_CODER_FLOAT:
		{
			float value;
			cc_coder_decode_float(data, &value);
			timer->timeout = (uint32_t)value;
			timer->last_triggered = cc_platform_get_seconds(obj->handler->calvinsys->node);
			timer->active = true;
			return CC_SUCCESS;
		}
		case CC_CODER_DOUBLE:
		{
			double value;
			cc_coder_decode_double(data, &value);
			timer->timeout = (uint32_t)value;
			timer->last_triggered = cc_platform_get_seconds(obj->handler->calvinsys->node);
			timer->active = true;
			return CC_SUCCESS;
		}
		case CC_CODER_UINT:
		{
			cc_coder_decode_uint(data, &timer->timeout);
			timer->last_triggered = cc_platform_get_seconds(obj->handler->calvinsys->node);
			timer->active = true;
			return CC_SUCCESS;
		}
		case CC_CODER_BOOL:
		{
			bool value = false;
			cc_coder_decode_bool(data, &value);
			if (value) {
				timer->last_triggered = cc_platform_get_seconds(obj->handler->calvinsys->node);
				timer->active = true;
			} else {
				timer->last_triggered = 0;
				timer->active = false;
			}
			return CC_SUCCESS;
		}
		break;
		default:
			cc_log_error("Unknown type");
	}

	return CC_FAIL;
}


static cc_result_t calvinsys_timer_close(struct cc_calvinsys_obj_t *obj)
{
	cc_platform_mem_free((void *)obj->state);
	obj->handler->objects = NULL; // TODO: Support multiple timers
	return CC_SUCCESS;
}

static cc_calvinsys_obj_t *calvinsys_timer_open(cc_calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char *capability_name)
{
	cc_calvinsys_obj_t *obj = NULL;
	cc_calvinsys_timer_t *timer = NULL;

	if (cc_platform_mem_alloc((void **)&timer, sizeof(cc_calvinsys_timer_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	if (cc_platform_mem_alloc((void **)&obj, sizeof(cc_calvinsys_obj_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		cc_platform_mem_free((void *)timer);
		return NULL;
	}

	timer->active = true;

	if (cc_coder_decode_uint_from_map(data, "timeout", &timer->timeout) != CC_SUCCESS) {
		cc_log_error("Failed to get attribute 'timeout'");
		cc_platform_mem_free((void *)timer);
		return NULL;
	}

	if (cc_coder_has_key(data, "last_triggered")) {
		if (cc_coder_decode_uint_from_map(data, "last_triggered", &timer->last_triggered) != CC_SUCCESS) {
			cc_log_error("Failed to get attribute 'last_triggered'");
			cc_platform_mem_free((void *)timer);
			return NULL;
		}
	} else
		timer->last_triggered = 0;

	cc_log("Timer created with timeout '%ld' s last triggered: '%ld'", timer->timeout, timer->last_triggered);

	obj->can_write = calvinsys_timer_can_write;
	obj->write = calvinsys_timer_write;
	obj->can_read = calvinsys_timer_can_read;
	obj->read = calvinsys_timer_read;
	obj->close = calvinsys_timer_close;
	obj->state = timer;

	return obj;
}

cc_result_t cc_calvinsys_timer_create(cc_calvinsys_t **calvinsys)
{
	cc_calvinsys_handler_t *handler = NULL;

	if (cc_platform_mem_alloc((void **)&handler, sizeof(cc_calvinsys_handler_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	handler->open = calvinsys_timer_open;
	handler->objects = NULL;
	handler->next = NULL;

	cc_calvinsys_add_handler(calvinsys, handler);
	if (cc_calvinsys_register_capability(*calvinsys, "sys.timer.once", handler, NULL) != CC_SUCCESS)
		return CC_FAIL;

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_timer_get_next_timeout(cc_node_t *node, uint32_t *timeout)
{
	uint32_t now = 0, diff = 0;
	cc_calvinsys_capability_t *timer_capability = (cc_calvinsys_capability_t *)cc_list_get(node->calvinsys->capabilities, "sys.timer.once");
	cc_calvinsys_obj_t *timer_obj = NULL;
	cc_calvinsys_timer_t *state = NULL;
	bool first = true;
	cc_result_t result = CC_FAIL;

	if (timer_capability != NULL) {
		timer_obj = timer_capability->handler->objects;
		*timeout = 0;
		now = cc_platform_get_seconds(node);
		timer_obj = timer_capability->handler->objects;
		while (timer_obj != NULL) {
			state = (cc_calvinsys_timer_t *)timer_obj->state;
			if (state->active) {
				if (((cc_calvinsys_timer_t *)timer_obj->state)->last_triggered == 0)
					diff = 0;
				else
					diff = ((cc_calvinsys_timer_t *)timer_obj->state)->timeout - (now - ((cc_calvinsys_timer_t *)timer_obj->state)->last_triggered);
				if (first) {
					*timeout = diff;
					first = false;
				} else {
					if (diff < *timeout)
						*timeout = diff;
				}
				result = CC_SUCCESS;
			}
			timer_obj = timer_obj->next;
		}
	}

	return result;
}
