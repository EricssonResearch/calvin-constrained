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
#ifndef CC_TRANSPORT_H
#define CC_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cc_common.h"
#ifdef CC_TLS_ENABLED
#include "../../crypto/cc_crypto.h"
#endif

#define CC_TRANSPORT_LEN_PREFIX_SIZE	4
#define CC_TRANSPORT_RX_BUFFER_SIZE		512

struct cc_node_t;

typedef enum {
	CC_TRANSPORT_INTERFACE_DOWN,
	CC_TRANSPORT_INTERFACE_UP,
	CC_TRANSPORT_CONNECTED,
	CC_TRANSPORT_DISCONNECTED,
	CC_TRANSPORT_PENDING,
	CC_TRANSPORT_ENABLED
} cc_transport_state_t;

typedef enum {
	CC_TRANSPORT_SOCKET_TYPE,
	CC_TRANSPORT_BT_TYPE,
	CC_TRANSPORT_FCM_TYPE,
	CC_TRANSPORT_SPRITZER_TYPE
} cc_transport_type_t;

typedef struct cc_transport_buffer_t {
	char *buffer;
	unsigned int pos;		// read/write pos
	unsigned int size;	// message size
} cc_transport_buffer_t;

/**
 * struct cc_transport_client_t - A transport client handing RT to RT communication
 * @transport_type: Type of transport
 * @uri: URI to connect to
 * @peer_id: ID of peer runtime
 * @state: current state
 * @rx_buffer: receive buffer
 * @client_state: implementation specific state
 * @prefix_len: the length of the prefix header
 * @crypto: TLS session data if enabled
 * @connect: function to connect to peer
 * @send: function to send data
 * @recv: function to receive data
 * @disconnect: function to disconnect from peer
 * @free: function to free cc_transport_client_t
 */
typedef struct cc_transport_client_t {
	cc_transport_type_t transport_type;
	char uri[CC_MAX_URI_LEN];
	char peer_id[CC_UUID_BUFFER_SIZE];
	volatile cc_transport_state_t state;
	cc_transport_buffer_t rx_buffer; // used to assemble fragmented messages
	void *client_state;
	uint8_t prefix_len;
#ifdef CC_TLS_ENABLED
	crypto_t crypto;
#endif
	cc_result_t (*connect)(struct cc_node_t *node, struct cc_transport_client_t *transport_client);
	int (*send)(struct cc_transport_client_t *transport_client, char *buffer, size_t size);
	int (*recv)(struct cc_transport_client_t *transport_client, char *buffer, size_t size);
	void (*disconnect)(struct cc_node_t *node, struct cc_transport_client_t *transport_client);
	void (*free)(struct cc_transport_client_t *transport_client);
} cc_transport_client_t;

/**
 * transcc_port_create() - Create a transport client for uri
 * @node the node object
 * @uri the uri to use when connecting
 *
 * @Return: the transport client object
 */
cc_transport_client_t *cc_transport_create(struct cc_node_t *node, char *uri);

/**
 * cc_transport_set_length_prefix() - Set 4 byte length prefix
 * @buffer the buffer to write to
 * @size the size of the data
 */
void cc_transport_set_length_prefix(char *buffer, size_t size);

/**
 * cc_transport_get_message_len() - Get message size from buffer
 * @buffer the data
 *
 * @Return: the size
 */
unsigned int cc_transport_get_message_len(const char *buffer);

/**
 * cc_transport_send() - Send data on transport client
 * @transport_client the transport client
 * @buffer the data to send
 * @size the size of the data to send
 *
 * @Return: SUCCESS/FAIL
 */
cc_result_t cc_transport_send(cc_transport_client_t *transport_client, char *buffer, int size);

/**
 * cc_transport_handle_data() - Reads data and calls handler when a compelete message has been received
 * @transport_client the transport client
 * @handler the handler to be called when a message has been received
 *
 * @Return: SUCCESS/FAIL
 */
cc_result_t cc_transport_handle_data(struct cc_node_t *node, cc_transport_client_t *transport_client, cc_result_t (*handler)(struct cc_node_t *node, char *data, size_t size));

/**
 * cc_transport_join() - Sends a JOIN_REQUEST to the connected peer
 * @node the node
 * @transport_client the transport client
 *
 * @Return: SUCCESS/FAIL
 */
cc_result_t cc_transport_join(struct cc_node_t *node, cc_transport_client_t *transport_client);

/**
 * cc_transport_disconnect() - Disconnect transport
 * @node the node
 * @transport_client the transport client
 */
void cc_transport_disconnect(struct cc_node_t *node, cc_transport_client_t *transport_client);

#endif /* CC_TRANSPORT_H */
