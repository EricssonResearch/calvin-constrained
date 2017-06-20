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

void calvinsys_add_handler(calvinsys_t **calvinsys, calvinsys_handler_t *handler)
{
	calvinsys_handler_t *tmp_handler = NULL;

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

void calvinsys_delete_handler(calvinsys_handler_t *handler)
{
	calvinsys_obj_t *obj = NULL;

	while (handler->objects != NULL) {
		obj = handler->objects;
		handler->objects = handler->objects->next;
		platform_mem_free((void *)obj);
	}

	platform_mem_free((void *)handler);
}

result_t calvinsys_register_capability(calvinsys_t *calvinsys, const char *name, calvinsys_handler_t *handler)
{
	if (list_get(calvinsys->capabilities, name) != NULL) {
		log_error("Capability '%s' already registered", name);
		return CC_RESULT_FAIL;
	}

	if (list_add_n(&calvinsys->capabilities, name, strlen(name), handler, sizeof(calvinsys_handler_t *)) == CC_RESULT_SUCCESS) {
		log("Capability '%s' registered", name);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

void calvinsys_delete_capabiltiy(calvinsys_t *calvinsys, const char *name)
{
	list_remove(&calvinsys->capabilities, name);
}

calvinsys_obj_t *calvinsys_open(calvinsys_t *calvinsys, const char *name, char *data, size_t size)
{
	calvinsys_handler_t *handler = NULL;
	calvinsys_obj_t *obj = NULL;

	handler = (calvinsys_handler_t *)list_get(calvinsys->capabilities, name);
	if (handler != NULL)
		obj = handler->open(handler, data, size);

	return obj;
}

void calvinsys_close(calvinsys_obj_t *obj)
{
	if (obj->close != NULL)
		obj->close(obj);
	obj->handler->objects = NULL; // TODO: Handle multiple objects
	platform_mem_free((void *)obj);
}
