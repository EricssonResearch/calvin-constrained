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
#include "runtime/south/platform/cc_platform.h"
#ifdef CC_TRANSPORT_SOCKET
#include "runtime/south/transport/socket/cc_transport_socket.h"
#endif
#ifdef CC_TRANSPORT_LWIP
#include "runtime/south/transport/lwip/cc_transport_lwip.h"
#endif
#ifdef CC_TRANSPORT_FCM
#include "runtime/south/transport/fcm/cc_transport_fcm.h"
#endif
#ifdef CC_TRANSPORT_SPRITZER
#include "runtime/south/transport/spritzer/cc_transport_spritzer.h"
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

cc_result_t cc_transport_handle_data(cc_node_t *node, cc_transport_client_t *transport_client, cc_result_t (*handler)(cc_node_t *node, char *data, size_t size))
{
	char prefix_buffer[4];
	int read = 0, msg_size = 0, to_read = 0;

	memset(prefix_buffer, 0, 4);

	while (true) {
		if (transport_client->rx_buffer.buffer != NULL)
			to_read = transport_client->rx_buffer.size - transport_client->rx_buffer.pos;
		else {
			read = cc_transport_recv(transport_client, prefix_buffer, 4);
			if (read < 4) {
				cc_log_error("Failed to read packet header, status '%d'", read);
				return CC_FAIL;
			}
			msg_size = cc_transport_get_message_len(prefix_buffer);

			if (cc_platform_mem_alloc((void **)&transport_client->rx_buffer.buffer, msg_size) != CC_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				return CC_FAIL;
			}

			transport_client->rx_buffer.pos = 0;
			transport_client->rx_buffer.size = msg_size;
			to_read = msg_size;
		}

		read = cc_transport_recv(transport_client, transport_client->rx_buffer.buffer + transport_client->rx_buffer.pos, to_read);
		if (read <= 0){
			cc_log_error("Failed to read payload, status '%d'", read);
			return CC_FAIL;
		}

		transport_client->rx_buffer.pos = transport_client->rx_buffer.pos + read;

		if (transport_client->rx_buffer.pos == transport_client->rx_buffer.size) {
			cc_log_debug("Transport: Packet received '%d' bytes", transport_client->rx_buffer.size);
			handler(node, transport_client->rx_buffer.buffer, transport_client->rx_buffer.size);
			cc_platform_mem_free(transport_client->rx_buffer.buffer);
			transport_client->rx_buffer.pos = 0;
			transport_client->rx_buffer.size = 0;
			transport_client->rx_buffer.buffer = NULL;
			return CC_SUCCESS;
		} else
			cc_log_debug("Transport: Fragment received");
	}
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
	if (transport_client == NULL) {
		cc_log_error("Transport client is NULL");
		return CC_FAIL;
	}

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
	cc_transport_client_t *client = NULL;

#ifdef CC_TRANSPORT_SOCKET
	if (strncmp(uri, "calvinip://", 11) == 0 || strncmp(uri, "ssdp", 4) == 0) {
		client = cc_transport_socket_create(node, uri);
	}
#endif

#ifdef CC_TRANSPORT_SPRITZER
	if (strncmp(uri, "calvinip://", 11) == 0)
		client = cc_transport_spritzer_create(node, uri);
#endif

#ifdef CC_TRANSPORT_LWIP
	if (strncmp(uri, "lwip", 4) == 0)
		client = cc_transport_lwip_create(node);
#endif

#ifdef CC_TRANSPORT_FCM
	if (strncmp(uri, "calvinfcm://", 12) == 0)
		client = transport_fcm_create(node, uri);
#endif

	if (client == NULL)
		cc_log_error("No transport for '%s'", uri);
	else {
		client->rx_buffer.pos = 0;
		client->rx_buffer.size = 0;
		client->rx_buffer.buffer = NULL;
	}

	return client;
}

void cc_transport_disconnect(struct cc_node_t *node, cc_transport_client_t *transport_client)
{
	transport_client->disconnect(node, transport_client);
	transport_client->state = CC_TRANSPORT_DISCONNECTED;
	transport_client->rx_buffer.pos = 0;
	transport_client->rx_buffer.size = 0;
	if (transport_client->rx_buffer.buffer != NULL) {
		cc_platform_mem_free(transport_client->rx_buffer.buffer);
		transport_client->rx_buffer.buffer = NULL;
	}
}
