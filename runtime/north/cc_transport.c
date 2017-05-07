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
#include <string.h>
#include <stdio.h>
#include "cc_transport.h"
#include "cc_node.h"
#include "cc_proto.h"
#include "../south/platform/cc_platform.h"
#ifdef CC_TRANSPORT_SOCKET
#include "../south/transport/socket/cc_transport_socket.h"
#endif
#ifdef CC_TRANSPORT_LWIP
#include "../south/transport/lwip/cc_transport_lwip.h"
#endif
#ifdef CC_TRANSPORT_FCM
#include "../south/transport/fcm/cc_transport_fcm.h"
#endif

unsigned int transport_get_message_len(const char *buffer)
{
	unsigned int value =
		((buffer[3] & 0xFF) <<  0) |
		((buffer[2] & 0xFF) <<  8) |
		((buffer[1] & 0xFF) << 16) |
		((buffer[0] & 0xFF) << 24);
	return value;
}

static int transport_recv(transport_client_t *transport_client, char *buffer, size_t size)
{
#ifdef CC_TLS_ENABLED
	return crypto_tls_recv(transport_client, buffer, size);
#endif
	return transport_client->recv(transport_client, buffer, size);
}

static void transport_free_transport_buffer(transport_buffer_t *buffer)
{
	platform_mem_free((void *)buffer->buffer);
	buffer->buffer = NULL;
	buffer->pos = 0;
	buffer->size = 0;
}

result_t transport_handle_data(node_t *node, transport_client_t *transport_client, result_t (*handler)(node_t *node, char *data, size_t size))
{
	char *rx_data = NULL, rx_buffer[TRANSPORT_RX_BUFFER_SIZE];
	int read = 0, read_pos = 0, msg_size = 0, to_read = 0;
	result_t result = CC_RESULT_SUCCESS;

	memset(rx_buffer, 0, TRANSPORT_RX_BUFFER_SIZE);

	// read until atleast one complete message has been received
	while (true) {
		if (read_pos == 0) {
			if (transport_client->rx_buffer.buffer != NULL) {
				rx_data = transport_client->rx_buffer.buffer + transport_client->rx_buffer.pos;
				to_read = transport_client->rx_buffer.size - transport_client->rx_buffer.pos;
			} else {
				rx_data = rx_buffer;
				to_read = TRANSPORT_RX_BUFFER_SIZE;
			}

			read = transport_recv(transport_client, rx_data, to_read);
			if (read <= 0) {
				log_error("Failed to read data from transport");
				return CC_RESULT_FAIL;
			}
		}

		if (transport_client->rx_buffer.buffer != NULL) {
			transport_client->rx_buffer.pos += read;
			// TODO: Handle trailing data
			if (transport_client->rx_buffer.size == transport_client->rx_buffer.pos) {
				result = handler(node, transport_client->rx_buffer.buffer, msg_size);
				transport_free_transport_buffer(&transport_client->rx_buffer);
				return result;
			}
		} else {
			msg_size = transport_get_message_len(rx_data + read_pos);
			read_pos += TRANSPORT_LEN_PREFIX_SIZE;
			if (msg_size == (read - read_pos)) {
				return handler(node, rx_data + read_pos, msg_size);
			} else if (msg_size > (read - read_pos)) {
				if (platform_mem_alloc((void **)&transport_client->rx_buffer.buffer, msg_size) != CC_RESULT_SUCCESS) {
					log_error("Failed to allocate memory");
					return CC_RESULT_FAIL;
				}

				memcpy(transport_client->rx_buffer.buffer, rx_buffer + read_pos, read - read_pos);
				transport_client->rx_buffer.size = msg_size;
				transport_client->rx_buffer.pos = read - read_pos;
				read_pos = 0;
				read = 0;
			} else {
				if (handler(node, rx_data + read_pos, msg_size) != CC_RESULT_SUCCESS)
					return CC_RESULT_FAIL;
				read_pos += msg_size;
			}
		}
	}

	return result;
}

void transport_set_length_prefix(char *buffer, size_t size)
{
	buffer[0] = size >> 24 & 0xFF;
	buffer[1] = size >> 16 & 0xFF;
	buffer[2] = size >> 8 & 0xFF;
	buffer[3] = size & 0xFF;
}

result_t transport_send(transport_client_t *transport_client, char *buffer, int size)
{
	transport_set_length_prefix(buffer, size - TRANSPORT_LEN_PREFIX_SIZE);

#ifdef CC_TLS_ENABLED
	if (crypto_tls_send(transport_client, buffer, size) == size)
		return CC_RESULT_SUCCESS;
	log_error("Failed to send TLS data");
#else
	if (transport_client->send(transport_client, buffer, size) == size)
		return CC_RESULT_SUCCESS;
	log_error("Failed to send data");
#endif

	return CC_RESULT_FAIL;
}

result_t transport_join(node_t *node, transport_client_t *transport_client)
{
#ifdef CC_TLS_ENABLED
	if (crypto_tls_init(node->id, transport_client) != CC_RESULT_SUCCESS) {
		log_error("Failed initialize TLS");
		transport_client->disconnect(node, transport_client);
		return CC_RESULT_FAIL;
	}
#endif

	if (proto_send_join_request(node, transport_client, SERIALIZER) != CC_RESULT_SUCCESS) {
		log_error("Failed to send join request");
		return CC_RESULT_FAIL;
	}

	transport_client->state = TRANSPORT_PENDING;

	return CC_RESULT_SUCCESS;
}

transport_client_t *transport_create(node_t *node, char *uri)
{
#ifdef CC_TRANSPORT_SOCKET
	if (strncmp(uri, "calvinip://", 11) == 0 || strncmp(uri, "ssdp", 4) == 0)
		return transport_socket_create(node, uri);
#endif

#ifdef CC_TRANSPORT_LWIP
	if (strncmp(uri, "lwip", 4) == 0)
		return transport_lwip_create(node);
#endif
#ifdef CC_TRANSPORT_FCM
	if (strncmp(uri, "calvinfcm://", 12) == 0)
		return transport_fcm_create(node, uri);
#endif

	log_error("No transport for '%s'", uri);

	return NULL;
}
