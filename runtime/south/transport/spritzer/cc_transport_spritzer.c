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
#include "net/lwip/net_lwip/lwip/sockets.h"
//#include "lwip/sys.h"
//#include "lwip/netdb.h"
#include "cc_transport_spritzer.h"
#include "runtime/north/cc_transport.h"
#include "runtime/south/platform/cc_platform.h"
#include "runtime/north/cc_node.h"

static cc_result_t cc_transport_spritzer_parse_uri(char *uri, char **ip, size_t *ip_len, int *port)
{
	size_t pos = strlen(uri);
	char *end = NULL;
	cc_result_t result = CC_FAIL;

	if (strncmp(uri, "calvinip://", 11) != 0) {
		cc_log_error("Failed to parse calvinip URI '%s'", uri);
		return CC_FAIL;
	}

	while (pos > 11) {
		if (uri[pos] == ':') {
			result = CC_SUCCESS;
			break;
		}
		pos--;
	}

	if (result == CC_SUCCESS) {
		*ip = uri + 11;
		*ip_len = pos - 11;
		*port = strtol(uri + pos + 1, &end, 10);
	}

	return result;
}

static int cc_transport_spritzer_send(cc_transport_client_t *transport_client, char *data, size_t size)
{
	return NT_Write(((cc_transport_spritzer_t *)transport_client->client_state)->fd, data, size);
}

static int cc_transport_spritzer_recv(cc_transport_client_t *transport_client, char *buffer, size_t size)
{
	return NT_Read(((cc_transport_spritzer_t *)transport_client->client_state)->fd, buffer, size);
}

static void cc_transport_spritzer_disconnect(cc_node_t *node, cc_transport_client_t *transport_client)
{
	if (transport_client->state != CC_TRANSPORT_DISCONNECTED) {
		close(((cc_transport_spritzer_t *)transport_client)->fd);
		transport_client->state = CC_TRANSPORT_DISCONNECTED;
	}

	if (transport_client->rx_buffer.buffer != NULL) {
		cc_platform_mem_free((void *)transport_client->rx_buffer.buffer);
    transport_client->rx_buffer.buffer = NULL;
  }
	transport_client->rx_buffer.pos = 0;
	transport_client->rx_buffer.size = 0;
}

static void cc_transport_spritzer_free(cc_transport_client_t *transport_client)
{
	if (transport_client->client_state != NULL)
		cc_platform_mem_free((void *)transport_client->client_state);
	cc_platform_mem_free((void *)transport_client);
}

static cc_result_t cc_transport_spritzer_connect(cc_node_t *node, cc_transport_client_t *transport_client)
{
	cc_transport_spritzer_t *transport = (cc_transport_spritzer_t *)transport_client->client_state;
	struct sockaddr_in server;

	transport->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (transport->fd < 0) {
		cc_log_error("Failed to create socket");
		return CC_FAIL;
	}

	server.sin_addr.s_addr = inet_addr(transport->ip);
	server.sin_port = htons(transport->port);
	server.sin_family = AF_INET;

	if (connect(transport->fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		cc_log_error("Failed to connect socket");
		return CC_FAIL;
	}

	transport_client->state = CC_TRANSPORT_CONNECTED;

	return CC_SUCCESS;
}

cc_transport_client_t *cc_transport_spritzer_create(cc_node_t *node, char *uri)
{
	cc_transport_client_t *transport_client = NULL;
	cc_transport_spritzer_t *transport = NULL;
	char *ip = NULL;
	size_t ip_len;
	int port = 0;

	if (cc_platform_mem_alloc((void **)&transport_client, sizeof(cc_transport_client_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	memset(transport_client, 0, sizeof(cc_transport_client_t));
	transport_client->transport_type = CC_TRANSPORT_SPRITZER_TYPE;
	transport_client->state = CC_TRANSPORT_INTERFACE_UP;
	transport_client->rx_buffer.buffer = NULL;
	transport_client->rx_buffer.pos = 0;
	transport_client->rx_buffer.size = 0;
	transport_client->connect = cc_transport_spritzer_connect;
	transport_client->send = cc_transport_spritzer_send;
	transport_client->recv = cc_transport_spritzer_recv;
	transport_client->disconnect = cc_transport_spritzer_disconnect;
	transport_client->free = cc_transport_spritzer_free;
	transport_client->prefix_len = CC_TRANSPORT_LEN_PREFIX_SIZE;

	if (cc_platform_mem_alloc((void **)&transport, sizeof(cc_transport_spritzer_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		cc_platform_mem_free((void *)transport_client);
		return NULL;
	}

	transport_client->client_state = transport;
	if (cc_transport_spritzer_parse_uri(uri, &ip, &ip_len, &port) != CC_SUCCESS) {
		cc_log_error("Failed to parse uri '%s'", uri);
		cc_platform_mem_free((void *)transport);
		cc_platform_mem_free((void *)transport_client);
		return NULL;
	}

	strncpy(transport_client->uri, uri, CC_MAX_URI_LEN);
	strncpy(transport->ip, ip, ip_len);
	transport->ip[ip_len] = '\0';
	transport->port = port;
	return transport_client;
}
