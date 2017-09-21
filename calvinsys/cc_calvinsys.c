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
#include "../runtime/north/cc_common.h"
#include "../runtime/south/platform/cc_platform.h"
#include "cc_calvinsys.h"

static cc_result_t cc_calvinsys_add_object_to_handler(cc_calvinsys_obj_t *obj, cc_calvinsys_handler_t *handler)
{
	cc_calvinsys_obj_t *tmp = handler->objects;

	if (tmp == NULL) {
		handler->objects = obj;
		return CC_SUCCESS;
	}

	while (tmp->next != NULL)
		tmp = tmp->next;
	tmp->next = obj;

	return CC_SUCCESS;
}

void cc_calvinsys_add_handler(cc_calvinsys_t **calvinsys, cc_calvinsys_handler_t *handler)
{
	cc_calvinsys_handler_t *tmp_handler = NULL;

	handler->calvinsys = *calvinsys;

	if ((*calvinsys)->handlers == NULL) {
		(*calvinsys)->handlers = handler;
	} else {
		tmp_handler = (*calvinsys)->handlers;
		while (tmp_handler->next != NULL)
			tmp_handler = tmp_handler->next;
		tmp_handler->next = handler;
	}
}

void cc_calvinsys_delete_handler(cc_calvinsys_handler_t *handler)
{
	cc_calvinsys_obj_t *obj = NULL;

	while (handler->objects != NULL) {
		obj = handler->objects;
		handler->objects = handler->objects->next;
		if (obj != NULL)
			cc_platform_mem_free((void *)obj);
	}

	cc_platform_mem_free((void *)handler);
}

cc_result_t cc_calvinsys_register_capability(cc_calvinsys_t *calvinsys, const char *name, cc_calvinsys_handler_t *handler, void *state)
{
	cc_calvinsys_capability_t *capability = NULL;

	if (cc_list_get(calvinsys->capabilities, name) != NULL) {
		cc_log_error("Capability '%s' already registered", name);
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&capability, sizeof(cc_calvinsys_capability_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	capability->handler = handler;
	capability->state = state;

	if (cc_list_add_n(&calvinsys->capabilities, name, strlen(name), capability, sizeof(cc_calvinsys_capability_t)) == CC_SUCCESS)
		return CC_SUCCESS;

	return CC_FAIL;
}

void cc_calvinsys_delete_capabiltiy(cc_calvinsys_t *calvinsys, const char *name)
{
	cc_calvinsys_capability_t *capability = NULL;

	capability = (cc_calvinsys_capability_t *)cc_list_get(calvinsys->capabilities, name);
	if (capability != NULL) {
		if (capability->state != NULL)
			cc_platform_mem_free(capability->state);
		cc_list_remove(&calvinsys->capabilities, name);
	}
}

cc_calvinsys_obj_t *cc_calvinsys_open(cc_calvinsys_t *calvinsys, const char *name, char *data, size_t size)
{
	cc_calvinsys_capability_t *capability = NULL;
	cc_calvinsys_obj_t *result = NULL;

	if (calvinsys == NULL) {
		cc_log_error("Actor does not have a calvinsys");
		return NULL;
	}

	capability = (cc_calvinsys_capability_t *)cc_list_get(calvinsys->capabilities, name);
	if (capability != NULL) {
		result = capability->handler->open(capability->handler, data, size, capability->state, calvinsys->next_id, name);
		if (result != NULL) {
			result->id = calvinsys->next_id++;
			result->handler = capability->handler;
			result->next = NULL;
			cc_calvinsys_add_object_to_handler(result, capability->handler);
		}
		return result;
	}
	return NULL;
}

void cc_calvinsys_close(cc_calvinsys_obj_t *obj)
{
	if (obj->close != NULL)
		obj->close(obj);
	obj->handler->objects = NULL; // TODO: Handle multiple objects
	cc_platform_mem_free((void *)obj);
}

cc_result_t cc_calvinsys_get_obj_by_id(cc_calvinsys_obj_t **obj, cc_calvinsys_handler_t *handler, uint32_t id)
{
	cc_calvinsys_obj_t *tmp = handler->objects;

	while (tmp != NULL) {
		if (tmp->id == id) {
			*obj = tmp;
			return CC_SUCCESS;
		}
		tmp = tmp->next;
	}
	return CC_FAIL;
}
