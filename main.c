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
#include <getopt.h>
#endif

int main(int argc, char **argv)
{
	char *name = NULL, *ssdp_iface = NULL, *proxy_iface = NULL;
	int vid = 1, pid = 1;
#ifndef NRF51
	int c = 0, proxy_port = 0;

	while ((c = getopt(argc, argv, "v:p:n:d:i:k:")) != -1) {
		switch (c) {
		case 'v':
			vid = atoi(optarg);
			break;
		case 'p':
			pid = atoi(optarg);
			break;
		case 'n':
			name = strdup(optarg);
			break;
		case 'd':
			ssdp_iface = strdup(optarg);
			break;
		case 'i':
			proxy_iface = strdup(optarg);
			break;
		case 'k':
			proxy_port = atoi(optarg);
			break;
		default:
			break;
		}
	}
#endif

	if (name == NULL)
		name = "constrained";

	if (ssdp_iface == NULL && proxy_iface == NULL)
		ssdp_iface = "0.0.0.0";

	platform_init();

	if (node_create(vid, pid, name) == SUCCESS) {
#ifdef NRF51
		// Node is started in platform_nrf51.c when interface is up to get mac address
		// of the connected peer.
		platform_run();
#else
		if (node_start(ssdp_iface, proxy_iface, proxy_port) == SUCCESS)
			node_run();
		else
			log_error("Failed to start node");
#endif
	} else
		log_error("Failed to create node");

	return EXIT_SUCCESS;
}
