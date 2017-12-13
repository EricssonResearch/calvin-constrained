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
#include "cc_actor_store.h"

static cc_result_t cc_actor_store_add(cc_list_t **actor_types, const char *name, cc_result_t (*func)(cc_actor_type_t *type))
{
	cc_actor_type_t *type = NULL;

	if (cc_platform_mem_alloc((void **)&type, sizeof(cc_actor_type_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	memset(type, 0, sizeof(cc_actor_type_t));

	if (func(type) != CC_SUCCESS) {
		cc_log_error("Failed to setup '%s'", name);
		cc_platform_mem_free(type);
		return CC_FAIL;
	}

	if (cc_list_add_n(actor_types, name, strlen(name), type, sizeof(cc_actor_type_t *)) == NULL) {
		cc_log_error("Failed to add '%s'", name);
		cc_platform_mem_free(type);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_store_init(cc_list_t **actor_types, uint8_t ntypes, cc_actor_builtin_type_t types[])
{
	uint8_t i = 0;

	for (i = 0; i < ntypes; i++) {
		if (cc_actor_store_add(actor_types, types[i].name, types[i].setup) != CC_SUCCESS)
			return CC_FAIL;
	}

	return CC_SUCCESS;
}
