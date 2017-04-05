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

#include "common.h"
#include "node.h"
#include "calvinsys.h"

result_t register_calvinsys(node_t* node, calvinsys_t* calvinsys)
{
	if (list_get(node->calvinsys, calvinsys->name) != NULL) {
		log_error("Calvinsys %s already registered", calvinsys->name);
		return FAIL;
	}
	return list_add(&node->calvinsys, calvinsys->name, calvinsys, sizeof(calvinsys_t*));
}

void unregister_calvinsys(node_t* node, calvinsys_t* calvinsys)
{
	list_remove(&node->calvinsys, calvinsys->name);
}

void release_calvinsys(calvinsys_t** calvinsys)
{
	if ((*calvinsys)->data != NULL)
		platform_mem_free((*calvinsys)->data);
	if ((*calvinsys)->command != NULL)
		platform_mem_free((*calvinsys)->command);
	platform_mem_free((*calvinsys)->name);
	platform_mem_free(*calvinsys);
	*calvinsys = NULL;
}
