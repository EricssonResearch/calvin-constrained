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
#ifdef MICROPYTHON
#include "libmpy/calvin_mpy_port.h"
#endif

int main(int argc, char **argv)
{
	char *name = NULL, *ssdp_iface = NULL, *proxy_iface = NULL, *capabilities = NULL;
	int proxy_port = 0;
#ifdef PARSE_ARGS
	int c = 0;
#ifdef LWM2M_HTTP_CLIENT
	int lwm2m_port = 0;
	char *lwm2m_iface = NULL, *lwm2m_url = NULL;
#endif
	static struct option long_options[] = {
		{"name", required_argument, NULL, 'a'},
		{"ssdp_iface", required_argument, NULL, 'b'},
		{"proxy_iface", required_argument, NULL, 'c'},
		{"proxy_port", required_argument, NULL, 'd'},
#ifdef LWM2M_HTTP_CLIENT
		{"lwm2m_iface", required_argument, NULL, 'e'},
		{"lwm2m_port", required_argument, NULL, 'f'},
		{"lwm2m_url", required_argument, NULL, 'g'},
#endif
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long (argc, argv, "a:b:c:d:e:f:g:", long_options, NULL)) != -1) {
		switch (c) {
		case 'a':
			name = strdup(optarg);
			break;
		case 'b':
			ssdp_iface = strdup(optarg);
			break;
		case 'c':
			proxy_iface = strdup(optarg);
			break;
		case 'd':
			proxy_port = atoi(optarg);
			break;
#ifdef LWM2M_HTTP_CLIENT
		case 'e':
			lwm2m_iface = strdup(optarg);
			break;
		case 'f':
			lwm2m_port = atoi(optarg);
			break;
		case 'g':
			lwm2m_url = strdup(optarg);
			break;
#endif
		default:
			break;
		}
	}
#endif

	platform_init();

#if defined(PARSE_ARGS) && defined(LWM2M_HTTP_CLIENT)
	if (lwm2m_url != NULL && strstr(lwm2m_url, "3303") != NULL)
		capabilities = "[3303]";

	platform_init_lwm2m(lwm2m_iface, lwm2m_port, lwm2m_url);
#endif

	if (name == NULL)
		name = "constrained";

	if (capabilities == NULL)
		capabilities = "[3303, 3201, 3200]";

	node_create(name, capabilities);

#if defined(PARSE_ARGS) && defined(LWM2M_HTTP_CLIENT)
	platform_init_lwm2m(lwm2m_iface, lwm2m_port, lwm2m_url);
#endif

#ifdef MICROPYTHON
	if (!mpy_port_init(MICROPYTHON_HEAP_SIZE))
		return 0;
#endif

	if (ssdp_iface == NULL && proxy_iface == NULL)
		ssdp_iface = "0.0.0.0";

	platform_run(ssdp_iface, proxy_iface, proxy_port);

	log("Exiting");

	return 0;
}
