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
#ifndef CC_TRANSPORT_LWIP_H
#define CC_TRANSPORT_LWIP_H

#include "lwip/tcp.h"
#include "runtime/north/cc_transport.h"

struct cc_node_t;

typedef struct cc_transport_lwip_rx_buffer_t {
	char buffer[CC_TRANSPORT_RX_BUFFER_SIZE];
	size_t size;
} cc_transport_lwip_rx_buffer_t;

typedef struct cc_transport_lwip_client_t {
	char mac[40];
	struct tcp_pcb *tcp_port;
	struct cc_node_t *node;
	cc_transport_lwip_rx_buffer_t rx_buffer;
} cc_transport_lwip_client_t;

cc_transport_client_t *cc_transport_lwip_get_client(void);
bool cc_transport_lwip_has_data(cc_transport_client_t *transport_client);

#endif /* CC_TRANSPORT_LWIP_H */
