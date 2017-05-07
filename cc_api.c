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
#include <unistd.h>
#include "cc_api.h"
#include "errno.h"
#ifdef CC_PYTHON_ENABLED
#include "libmpy/cc_mpy_port.h"
#endif

result_t api_runtime_init(node_t **node, char *name, char *proxy_uris, char *storage_dir)
{
	platform_init();

#ifdef CC_PYTHON_ENABLED
	if (!mpy_port_init(CC_PYTHON_HEAP_SIZE)) {
		log_error("Failed to initialize micropython lib");
		return CC_RESULT_FAIL;
	}
#endif

	if (platform_mem_alloc((void **)node, sizeof(node_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}
	memset(*node, 0, sizeof(node_t));
	(*node)->storage_dir = storage_dir;
	(*node)->attributes = NULL;

	return node_init(*node, name, proxy_uris);
}

result_t api_runtime_start(node_t *node)
{
	node_run(node);
	platform_stop(node);
	return CC_RESULT_SUCCESS;
}

result_t api_runtime_stop(node_t *node)
{
	node->state = NODE_STOP;
	platform_mem_free((void *)node);
	return CC_RESULT_SUCCESS;
}

result_t api_runtime_serialize_and_stop(node_t *node)
{
#ifdef CC_STORAGE_ENABLED
	if (node->state == NODE_STARTED)
		node_set_state(node);
#endif
	return api_runtime_stop(node);
}

#ifdef CC_STORAGE_ENABLED
result_t api_clear_serialization_file(char *filedir)
{
	char *filename = "calvinconstrained.config";
	char abs_filepath[strlen(filename) + strlen(filedir) + 1];

	strcpy(abs_filepath, filedir);
	if (filedir[strlen(filedir)-1] != '/')
		strcat(abs_filepath, "/");
	strcat(abs_filepath, filename);
	if (unlink(abs_filepath) < 0)
		return CC_RESULT_FAIL;
	return CC_RESULT_SUCCESS;
}
#endif

result_t api_reconnect(node_t *node)
{
	if (node->transport_client->state == TRANSPORT_ENABLED)
		node->transport_client->state = TRANSPORT_INTERFACE_DOWN;
	return CC_RESULT_SUCCESS;
}
