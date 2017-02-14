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
#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "common.h"
#ifdef USE_LWIP
#include "lwip/tcp.h"
#endif

struct node_t;

typedef enum {
	TRANSPORT_INTERFACE_DOWN,
	TRANSPORT_DISCONNECTED,
	TRANSPORT_DO_JOIN,
	TRANSPORT_PENDING,
	TRANSPORT_ENABLED
} transport_state_t;

typedef struct transport_buffer_t {
	char *buffer;
	unsigned int pos;
	unsigned int size;
} transport_buffer_t;

typedef struct transport_client_t {
#ifdef USE_LWIP
	struct tcp_pcb *tcp_port;
	struct node_t *node;
#else
	int fd;
	bool do_discover;
#endif
	char uri[100];
	bool has_pending_tx;
	transport_state_t state;
	transport_buffer_t rx_buffer;
	transport_buffer_t tx_buffer;
} transport_client_t;

transport_client_t *transport_create(struct node_t *node, char *uri);
result_t transport_connect(transport_client_t *transport_client);
result_t transport_send(transport_client_t *transport_client, size_t size);
void transport_disconnect(transport_client_t *transport_client);
#ifdef USE_LWIP
transport_client_t *transport_get_client(void);
#endif

#endif /* TRANSPORT_H */
