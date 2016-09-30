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

#include "common.h"
#include <stdint.h>
#include <stddef.h>
#ifdef NRF51
#include "lwip/tcp.h"
#endif

typedef enum {
	TRANSPORT_DISCONNECTED,
	TRANSPORT_CONNECTED,
	TRANSPORT_JOINED
} transport_state_t;

#ifdef NRF51
typedef enum {
	TCP_STATE_IDLE,
	TCP_STATE_REQUEST_CONNECTION,
	TCP_STATE_CONNECTED,
	TCP_STATE_DATA_TX_IN_PROGRESS,
	TCP_STATE_TCP_SEND_PENDING,
	TCP_STATE_DISCONNECTED
} tcp_state_t;

typedef struct send_buffer_t {
	char *data;
	unsigned int length;
	struct send_buffer_t *next;
} send_buffer_t;
#endif

typedef struct transport_client_t {
	transport_state_t state;
#ifdef NRF51
	struct tcp_pcb *tcp_port;
	tcp_state_t tcp_state;
	send_buffer_t *send_list;
#else
	int fd;
#endif
	int msg_size;
	char *buffer;
	int buffer_pos;
} transport_client_t;

result_t discover_proxy(const char *iface, char *ip, int *port);
transport_client_t *client_connect(const char *address, int port);
result_t client_send(const transport_client_t *client, char *data, size_t len);
result_t wait_for_data(transport_client_t **client, uint32_t timeout);
void free_client(transport_client_t *client);

#endif /* TRANSPORT_H */
