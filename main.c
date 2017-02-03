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
#include "platform.h"
#include "node.h"
#ifdef PARSE_ARGS
#include <string.h>
#include <getopt.h>
#endif
#ifdef MICROPYTHON
#include "libmpy/calvin_mpy_port.h"
#endif

int main(int argc, char **argv)
{
	char *name = NULL, *iface = NULL;
	int port = 0;
	node_t *node = NULL;
#ifdef PARSE_ARGS
	int c = 0;
	static struct option long_options[] = {
		{"name", required_argument, NULL, 'n'},
		{"iface", required_argument, NULL, 'i'},
		{"port", required_argument, NULL, 'p'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long (argc, argv, "n:i:p:", long_options, NULL)) != -1) {
		switch (c) {
		case 'n':
			name = strdup(optarg);
			break;
		case 'i':
			iface = strdup(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		default:
			break;
		}
	}
#endif

	platform_init();

#ifdef MICROPYTHON
	if (!mpy_port_init(MICROPYTHON_HEAP_SIZE))
		return EXIT_FAILURE;
#endif

	if (name == NULL)
		name = strdup("constrained");

	node = node_create(name);
	if (node != NULL)
		platform_run(node, iface, port);

	return EXIT_SUCCESS;
}
