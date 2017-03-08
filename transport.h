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

#ifdef PLATFORM_ANDROID
#define BUFFER_SIZE					4096
#else
#define BUFFER_SIZE					512
#endif

struct node_t;

typedef enum {
	TRANSPORT_INTERFACE_DOWN,
	TRANSPORT_INTERFACE_UP,
	TRANSPORT_DISCONNECTED,
	TRANSPORT_PENDING,
	TRANSPORT_ENABLED
} transport_state_t;

typedef struct transport_buffer_t {
	char *buffer;
	unsigned int pos;
	unsigned int size;
} transport_buffer_t;

typedef struct transport_client_t {
    char uri[100];
	char peer_id[UUID_BUFFER_SIZE];
	transport_state_t state;
	transport_buffer_t rx_buffer;
	transport_buffer_t tx_buffer;
	void *client_state;
	result_t (*connect)(struct node_t *node, struct transport_client_t *transport_client);
	result_t (*send_tx_buffer)(const struct node_t *node, struct transport_client_t *transport_client, size_t size);
	void (*disconnect)(struct node_t* node, struct transport_client_t *transport_client);
	void (*free)(struct transport_client_t *transport_client);
} transport_client_t;

transport_client_t *transport_create(struct node_t *node, char *uri);
void transport_handle_data(struct node_t *node, transport_client_t *transport_client, char *buffer, size_t size);
result_t transport_create_tx_buffer(transport_client_t *transport_client, size_t size);
void transport_free_tx_buffer(transport_client_t *transport_client);
void transport_append_buffer_prefix(char *buffer, size_t size);
void transport_join(struct node_t *node, transport_client_t *transport_client);
unsigned int get_message_len(const char *buffer);

#endif /* TRANSPORT_H */
