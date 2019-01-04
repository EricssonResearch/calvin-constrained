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
#include "cc_calvinsys_schedule.h"
#include "runtime/north/cc_common.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/coder/cc_coder.h"

static bool cc_calvinsys_schedule_can_read(struct cc_calvinsys_obj_t *obj)
{
	cc_calvinsys_schedule_t *schedule = (cc_calvinsys_schedule_t *)obj->state;

	return schedule->triggered;
}

static cc_result_t cc_calvinsys_schedule_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	cc_calvinsys_schedule_t *schedule = (cc_calvinsys_schedule_t *)obj->state;

	schedule->triggered = false;

	return CC_SUCCESS;
}

static bool cc_calvinsys_schedule_can_write(struct cc_calvinsys_obj_t *obj)
{
	cc_calvinsys_schedule_t *schedule = (cc_calvinsys_schedule_t *)obj->state;

	return !schedule->triggered;
}

static cc_result_t cc_calvinsys_schedule_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	cc_calvinsys_schedule_t *schedule = (cc_calvinsys_schedule_t *)obj->state;

	schedule->triggered = true;

	return CC_SUCCESS;
}

static cc_result_t cc_calvinsys_schedule_close(struct cc_calvinsys_obj_t *obj)
{
	cc_platform_mem_free((void *)obj->state);
	return CC_SUCCESS;
}

static cc_result_t cc_calvinsys_schedule_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	cc_calvinsys_schedule_t *schedule = NULL;

	if (cc_platform_mem_alloc((void **)&schedule, sizeof(cc_calvinsys_schedule_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	schedule->triggered = true;

	obj->can_write = cc_calvinsys_schedule_can_write;
	obj->write = cc_calvinsys_schedule_write;
	obj->can_read = cc_calvinsys_schedule_can_read;
	obj->read = cc_calvinsys_schedule_read;
	obj->close = cc_calvinsys_schedule_close;
	obj->serialize = NULL;
	obj->state = schedule;

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_schedule_create(cc_calvinsys_t **calvinsys)
{
	if (cc_calvinsys_create_capability(*calvinsys, "sys.schedule",
			cc_calvinsys_schedule_open,
			cc_calvinsys_schedule_open,
			NULL,
			false) != CC_SUCCESS) {
		cc_log_error("Failed to create 'sys.schedule'");
	 	return CC_FAIL;
	}

	return CC_SUCCESS;
}
