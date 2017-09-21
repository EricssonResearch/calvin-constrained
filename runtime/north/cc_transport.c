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
#include "coder/cc_coder.h"
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

unsigned int cc_transport_get_message_len(const char *buffer)
{
	unsigned int value =
		((buffer[3] & 0xFF) <<  0) |
		((buffer[2] & 0xFF) <<  8) |
		((buffer[1] & 0xFF) << 16) |
		((buffer[0] & 0xFF) << 24);
	return value;
}

static int cc_transport_recv(cc_transport_client_t *transport_client, char *buffer, size_t size)
{
#ifdef CC_TLS_ENABLED
	return crypto_tls_recv(transport_client, buffer, size);
#endif
	return transport_client->recv(transport_client, buffer, size);
}

static void cc_transport_free_transport_buffer(cc_transport_buffer_t *buffer)
{
	cc_platform_mem_free((void *)buffer->buffer);
	buffer->buffer = NULL;
	buffer->pos = 0;
	buffer->size = 0;
}

cc_result_t cc_transport_handle_data(cc_node_t *node, cc_transport_client_t *transport_client, cc_result_t (*handler)(cc_node_t *node, char *data, size_t size))
{
	char *rx_data = NULL, rx_buffer[CC_TRANSPORT_RX_BUFFER_SIZE];
	int read = 0, read_pos = 0, msg_size = 0, to_read = 0;
	cc_result_t result = CC_SUCCESS;

	memset(rx_buffer, 0, CC_TRANSPORT_RX_BUFFER_SIZE);

	// read until atleast one complete message has been received
	while (true) {
		if (read_pos == 0) {
			if (transport_client->rx_buffer.buffer != NULL) {
				rx_data = transport_client->rx_buffer.buffer + transport_client->rx_buffer.pos;
				to_read = transport_client->rx_buffer.size - transport_client->rx_buffer.pos;
			} else {
				rx_data = rx_buffer;
				to_read = CC_TRANSPORT_RX_BUFFER_SIZE;
			}

			read = cc_transport_recv(transport_client, rx_data, to_read);
			if (read <= 0) {
				cc_log_error("Failed to read data from transport");
				transport_client->state = CC_TRANSPORT_DISCONNECTED;
				return CC_FAIL;
			}
		}

		if (transport_client->rx_buffer.buffer != NULL) {
			transport_client->rx_buffer.pos += read;
			// TODO: Handle trailing data
			if (transport_client->rx_buffer.size == transport_client->rx_buffer.pos) {
				result = handler(node, transport_client->rx_buffer.buffer, msg_size);
				cc_transport_free_transport_buffer(&transport_client->rx_buffer);
				return result;
			}
		} else {
			msg_size = cc_transport_get_message_len(rx_data + read_pos);
			read_pos += CC_TRANSPORT_LEN_PREFIX_SIZE;
			if (msg_size == (read - read_pos)) {
				return handler(node, rx_data + read_pos, msg_size);
			} else if (msg_size > (read - read_pos)) {
				if (cc_platform_mem_alloc((void **)&transport_client->rx_buffer.buffer, msg_size) != CC_SUCCESS) {
					cc_log_error("Failed to allocate memory");
					return CC_FAIL;
				}

				memcpy(transport_client->rx_buffer.buffer, rx_buffer + read_pos, read - read_pos);
				transport_client->rx_buffer.size = msg_size;
				transport_client->rx_buffer.pos = read - read_pos;
				read_pos = 0;
				read = 0;
			} else {
				if (handler(node, rx_data + read_pos, msg_size) != CC_SUCCESS)
					return CC_FAIL;
				read_pos += msg_size;
			}
		}
	}

	return result;
}

void cc_transport_set_length_prefix(char *buffer, size_t size)
{
	buffer[0] = size >> 24 & 0xFF;
	buffer[1] = size >> 16 & 0xFF;
	buffer[2] = size >> 8 & 0xFF;
	buffer[3] = size & 0xFF;
}

cc_result_t cc_transport_send(cc_transport_client_t *transport_client, char *buffer, int size)
{
	cc_transport_set_length_prefix(buffer, size - CC_TRANSPORT_LEN_PREFIX_SIZE);

#ifdef CC_TLS_ENABLED
	if (crypto_tls_send(transport_client, buffer, size) == size)
		return CC_SUCCESS;
	cc_log_error("Failed to send TLS data");
#else
	if (transport_client->send(transport_client, buffer, size) == size)
		return CC_SUCCESS;
	cc_log_error("Failed to send data");
#endif

	return CC_FAIL;
}

cc_result_t cc_transport_join(cc_node_t *node, cc_transport_client_t *transport_client)
{
	char *serializer = NULL;

#ifdef CC_TLS_ENABLED
	if (crypto_tls_init(node->id, transport_client) != CC_SUCCESS) {
		cc_log_error("Failed initialize TLS");
		transport_client->disconnect(node, transport_client);
		return CC_FAIL;
	}
#endif

	serializer = cc_coder_get_name();
	if (serializer == NULL) {
		cc_log_error("Failed to get serializer name");
		return CC_FAIL;
	}

	if (cc_proto_send_join_request(node, transport_client, serializer) != CC_SUCCESS) {
		cc_log_error("Failed to send join request");
		cc_platform_mem_free((void *)serializer);
		return CC_FAIL;
	}

	transport_client->state = CC_TRANSPORT_PENDING;
	cc_platform_mem_free((void *)serializer);

	return CC_SUCCESS;
}

cc_transport_client_t *cc_transport_create(cc_node_t *node, char *uri)
{
#ifdef CC_TRANSPORT_SOCKET
	if (strncmp(uri, "calvinip://", 11) == 0 || strncmp(uri, "ssdp", 4) == 0)
		return cc_transport_socket_create(node, uri);
#endif

#ifdef CC_TRANSPORT_LWIP
	if (strncmp(uri, "lwip", 4) == 0)
		return cc_transport_lwip_create(node);
#endif
#ifdef CC_TRANSPORT_FCM
	if (strncmp(uri, "calvinfcm://", 12) == 0)
		return transport_fcm_create(node, uri);
#endif

	cc_log_error("No transport for '%s'", uri);

	return NULL;
}
