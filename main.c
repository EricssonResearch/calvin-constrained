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
#include "platform.h"
#include "node.h"
#ifndef NRF51
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#ifdef NRF51
int main(void)
{
    if (create_node(1, 1, "nRF51", "fe80::21a:7dff:feda:710b", 5000) == SUCCESS) {
    	platform_init();
    }
    return 0;
}
#else
int main(int argc, char *argv[])
{
	char *name = NULL, *address = NULL;
	int c = -1, port = 0;

	while ((c = getopt(argc, argv, "n:i:p:")) != -1) {
	        switch (c) {
	        case 'n':
	                name = strdup(optarg);
	                break;
	        case 'i':
	                address = strdup(optarg);
	                break;
	        case 'p':
	                port = atoi(optarg);
	                break;
	        default:
	                printf("Usage: %s -n NAME -i PROXY_ADDRESS -p PROXY_PORT\n", argv[0]);
	                break;
	    }
	}

	if (name != NULL && address != NULL && port != 0) {
		if (create_node(1, 1, name, address, port) == SUCCESS) {
			if (start_node() == SUCCESS) {
				node_run();
			} else {
				log_error("Failed to start node");
			}
		} else {
			log_error("Failed to create node");
		}
	} else {
		printf("Usage: %s -n NAME -i PROXY_ADDRESS -p PROXY_PORT\n", argv[0]);
	}

    return 0;
}
#endif
