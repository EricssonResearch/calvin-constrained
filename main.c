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
#ifdef PARSE_ARGS
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#endif

int main(int argc, char **argv)
{
	char *name = NULL, *ssdp_iface = NULL, *proxy_iface = NULL;
	int vid = 1, pid = 1, proxy_port = 0;
#ifdef PARSE_ARGS
	int c = 0;
#ifdef LWM2M_HTTP_CLIENT
	int lwm2m_port = 0;
	char *lwm2m_iface = NULL, *lwm2m_endpoint = NULL;
#endif
	static struct option long_options[] = {
		{"vid", required_argument, NULL, 'a'},
		{"pid", required_argument, NULL, 'b'},
		{"name", required_argument, NULL, 'c'},
		{"ssdp_iface", required_argument, NULL, 'd'},
		{"proxy_iface", required_argument, NULL, 'e'},
		{"proxy_port", required_argument, NULL, 'f'},
#ifdef LWM2M_HTTP_CLIENT
		{"lwm2m_iface", required_argument, NULL, 'g'},
		{"lwm2m_port", required_argument, NULL, 'h'},
		{"lwm2m_endpoint", required_argument, NULL, 'i'},
#endif
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long (argc, argv, "a:b:c:d:e:f:g:h:i:", long_options, NULL)) != -1) {
		switch (c) {
		case 'a':
			vid = atoi(optarg);
			break;
		case 'b':
			pid = atoi(optarg);
			break;
		case 'c':
			name = strdup(optarg);
			break;
		case 'd':
			ssdp_iface = strdup(optarg);
			break;
		case 'e':
			proxy_iface = strdup(optarg);
			break;
		case 'f':
			proxy_port = atoi(optarg);
			break;
#ifdef LWM2M_HTTP_CLIENT
		case 'g':
			lwm2m_iface = strdup(optarg);
			break;
		case 'h':
			lwm2m_port = atoi(optarg);
			break;
		case 'i':
			lwm2m_endpoint = strdup(optarg);
			break;
#endif
		default:
			break;
		}
	}
#endif

	if (name == NULL)
		name = "constrained";

	node_create(vid, pid, name);

	platform_init();
#if defined(PARSE_ARGS) && defined(LWM2M_HTTP_CLIENT)
	platform_init_lwm2m(lwm2m_iface, lwm2m_port, lwm2m_endpoint);
#endif

	if (ssdp_iface == NULL && proxy_iface == NULL)
		ssdp_iface = "0.0.0.0";

	if (platform_run(ssdp_iface, proxy_iface, proxy_port) != SUCCESS)
		log_error("Exiting");

	return 0;
}
