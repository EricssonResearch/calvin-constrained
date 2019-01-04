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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cc_config.h"
#include "cc_api.h"
#if (CC_USE_GETOPT == 1)
#include <getopt.h>
#endif

int main(int argc, char **argv)
{
	char *attr = NULL, *uris = NULL, *platform_args = NULL;
	char *script = NULL;
	cc_node_t *node = NULL;
#if (CC_USE_GETOPT == 1)
	int c = 0;
	static struct option long_options[] = {
		{"attr", optional_argument, NULL, 'a'},
		{"uris", optional_argument, NULL, 'u'},
		{"platform_data", optional_argument, NULL, 'p'},
		{"script", optional_argument, NULL, 's'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "a:u:p:s:", long_options, NULL)) != -1) {
		switch (c) {
		case 'a':
			attr = optarg;
			break;
		case 'u':
			uris = optarg;
			break;
		case 'p':
			platform_args = optarg;
			break;
		case 's':
			script = optarg;
			break;
		default:
			break;
		}
	}
#endif

	if (cc_api_runtime_init(&node, attr, uris, platform_args) != CC_SUCCESS)
		return EXIT_FAILURE;

	if (cc_api_runtime_start(node, script) != CC_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
