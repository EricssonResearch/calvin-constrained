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
#include "cc_config.h"
#include "cc_api.h"
#include "errno.h"
#if (CC_USE_PYTHON == 1)
#include "libmpy/cc_mpy_port.h"
#endif

cc_result_t cc_api_runtime_init(cc_node_t **node, const char *attributes, const char *uris, const char *platform_args)
{
	cc_platform_early_init();

	if (cc_platform_mem_alloc((void **)node, sizeof(cc_node_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	memset(*node, 0, sizeof(cc_node_t));

#if (CC_USE_PYTHON == 1)
	// MicroPython is deinitialized and its heap is freed in node_free
	// to be freed when enterring sleep
	if (cc_platform_mem_alloc(&(*node)->mpy_heap, CC_PYTHON_HEAP_SIZE) != CC_SUCCESS) {
		cc_log_error("Failed to allocate MicroPython heap");
		return CC_FAIL;
	}
	memset((*node)->mpy_heap, 0, CC_PYTHON_HEAP_SIZE);

	if (!cc_mpy_port_init((*node)->mpy_heap, CC_PYTHON_HEAP_SIZE, CC_PYTHON_STACK_SIZE)) {
		cc_log_error("Failed to init MicroPython");
		return CC_FAIL;
	}

	cc_log("MicroPython initialized, heap size '%d' stack size '%d'",
		CC_PYTHON_HEAP_SIZE,
		CC_PYTHON_STACK_SIZE);
#endif

	if (cc_node_init(*node, attributes, uris) != CC_SUCCESS) {
		cc_log_error("Failed to init node");
		return CC_FAIL;
	}

	if (cc_platform_late_init(*node, platform_args) != CC_SUCCESS) {
		cc_log_error("Failed to init platform");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_api_runtime_start(cc_node_t *node, const char *script)
{
	cc_node_run(node, script);
	return CC_SUCCESS;
}

cc_result_t cc_api_runtime_stop(cc_node_t *node)
{
	node->state = CC_NODE_STOP;
	return CC_SUCCESS;
}

cc_result_t cc_api_runtime_serialize_and_stop(cc_node_t *node)
{
#if (CC_USE_STORAGE == 1)
	if (node->state == CC_NODE_STARTED)
		cc_node_set_state(node, true);
#endif
	return cc_api_runtime_stop(node);
}

#if (CC_USE_STORAGE == 1)
cc_result_t cc_api_clear_serialization_file(char *filedir)
{
	char abs_filepath[strlen(CC_STATE_FILE) + strlen(filedir) + 1];

	strcpy(abs_filepath, filedir);
	if (filedir[strlen(filedir) - 1] != '/')
		strcat(abs_filepath, "/");
	strcat(abs_filepath, CC_STATE_FILE);
	if (unlink(abs_filepath) < 0)
		return CC_FAIL;
	return CC_SUCCESS;
}
#endif

cc_result_t cc_api_reconnect(cc_node_t *node)
{
	if (node->transport_client->state == CC_TRANSPORT_ENABLED)
		node->transport_client->state = CC_TRANSPORT_INTERFACE_DOWN;
	return CC_SUCCESS;
}
