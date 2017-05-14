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
#include "../runtime/north/cc_node.h"
#include "cc_calvinsys.h"

result_t calvinsys_register_handler(list_t **calvinsys, const char *name, calvinsys_handler_t *handler)
{
	if (list_get(*calvinsys, name) != NULL) {
		log_error("Calvinsys '%s' handler already registered", name);
		return CC_RESULT_FAIL;
	}

	log("Calvinsys '%s' registered", name);
	return list_add_n(calvinsys, name, strlen(name), handler, sizeof(calvinsys_handler_t *));
}

void calvinsys_free_handler(node_t *node, char *name)
{
	calvinsys_handler_t *handler = NULL;
	calvinsys_obj_t *obj = NULL;

	handler = (calvinsys_handler_t *)list_get(node->calvinsys, name);
	if (handler != NULL) {
		while (handler->objects != NULL) {
			obj = handler->objects;
			handler->objects = handler->objects->next;
			platform_mem_free((void *)obj);
		}
		platform_mem_free((void *)handler);
		list_remove(&node->calvinsys, name);
	}
}

calvinsys_obj_t *calvinsys_open(list_t *calvinsys, const char *name, char *data, size_t size)
{
	calvinsys_handler_t *handler = NULL;
	calvinsys_obj_t *obj = NULL;

	handler = (calvinsys_handler_t *)list_get(calvinsys, name);
	if (handler != NULL)
		obj = handler->open(handler, data, size);

	return obj;
}

void calvinsys_close(calvinsys_obj_t *obj)
{
	if (obj->close != NULL)
		obj->close(obj);
	obj->handler->objects = NULL; // TODO: Handle multiple objects and rearrange list
	platform_mem_free((void *)obj);
}
