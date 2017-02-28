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
#include "platform.h"
#include "node.h"
#ifdef PARSE_ARGS
#include <getopt.h>
#endif
#ifdef MICROPYTHON
#include "libmpy/calvin_mpy_port.h"
#endif

int main(int argc, char **argv)
{
	char *name = NULL, *proxy_uris = NULL;
	node_t node = {};
#ifdef PARSE_ARGS
	int c = 0;
	static struct option long_options[] = {
		{"name", required_argument, NULL, 'n'},
		{"proxy_uris", required_argument, NULL, 'p'},
		{NULL, 0, NULL, 0}
	};
#endif

	platform_init(&node);

#ifdef PARSE_ARGS
	while ((c = getopt_long(argc, argv, "n:p:", long_options, NULL)) != -1) {
		switch (c) {
		case 'n':
			name = optarg;
			break;
		case 'p':
			proxy_uris = optarg;
			break;
		default:
			break;
		}
	}

	if (name == NULL) {
		log_error("Missing argument 'name'");
		return EXIT_FAILURE;
	}

	if (proxy_uris == NULL) {
		log_error("Missing argument 'proxy_uris'");
		return EXIT_FAILURE;
	}
#endif

#ifdef MICROPYTHON
	if (!mpy_port_init(MICROPYTHON_HEAP_SIZE))
		return EXIT_FAILURE;
#endif

	node_run(&node, name, proxy_uris);

	return EXIT_SUCCESS;
}
