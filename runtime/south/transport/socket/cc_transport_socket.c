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
#ifdef CC_LWIP_SOCKET
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#ifdef CC_PLATFORM_ANDROID
#include <sys/socket.h>
#endif
#endif
#include <string.h>
#include "cc_transport_socket.h"
#include "../../../north/cc_transport.h"
#include "../../../south/platform/cc_platform.h"
#include "../../../north/cc_node.h"

#ifdef CC_TRANSPORT_SOCKET_SSDP_ENABLED
#define LOCATION_SIZE	100
#define URL_SIZE	100
#define URI_SIZE	100
#define ADDRESS_SIZE	20
#define IP_SIZE		20
#define SSDP_MULTICAST	"239.255.255.250"
#define SSDP_PORT	1900
#define SERVICE_UUID	"1693326a-abb9-11e4-8dfb-9cb654a16426"
#define RECV_BUF_SIZE	512

static result_t transport_socket_discover_location(char *location)
{
	int sock = 0;
	size_t ret = 0;
	unsigned int socklen = 0;
	int len = 0;
	struct sockaddr_in sockname;
	struct sockaddr clientsock;
	char buffer[RECV_BUF_SIZE];
	fd_set fds;
	char *location_start = NULL;
	char *location_end = NULL;
	struct timeval tv = {5, 0};

	len = snprintf(buffer, RECV_BUF_SIZE, "M-SEARCH * HTTP/1.1\r\n" \
		"HOST: %s:%d\r\n" \
		"MAN: \"ssdp:discover\"\r\n" \
		"MX: 2\r\n" \
		"ST: uuid:%s\r\n" \
		"\r\n",
		SSDP_MULTICAST, SSDP_PORT, SERVICE_UUID);

	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		cc_log_error("Failed to send ssdp request");
		return CC_RESULT_FAIL;
	}

	memset((char *)&sockname, 0, sizeof(struct sockaddr_in));
	sockname.sin_family = AF_INET;
	sockname.sin_port = htons(SSDP_PORT);
	sockname.sin_addr.s_addr = inet_addr(SSDP_MULTICAST);

	cc_log("Sending ssdp request");

	ret = sendto(sock, buffer, len, 0, (struct sockaddr *) &sockname, sizeof(struct sockaddr_in));
	if (ret != len) {
		cc_log_error("Failed to send ssdp request");
		return CC_RESULT_FAIL;
	}

	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	if (select(sock + 1, &fds, NULL, NULL, &tv) <= 0) {
		cc_log_debug("No ssdp response");
		return CC_RESULT_FAIL;
	}

	if (FD_ISSET(sock, &fds)) {
		socklen = sizeof(clientsock);
		len = recvfrom(sock, buffer, RECV_BUF_SIZE, MSG_PEEK, &clientsock, &socklen);
		if (len == -1) {
			cc_log_error("Failed to receive ssdp response");
			return CC_RESULT_FAIL;
		}

		buffer[len] = '\0';
		close(sock);

		if (strncmp(buffer, "HTTP/1.1 200 OK", 15) != 0) {
			cc_log_error("Bad ssdp response");
			return CC_RESULT_FAIL;
		}

		location_start = strnstr(buffer, "LOCATION:", len);
		if (location_start != NULL) {
			location_start = location_start + 10;
			location_end = strnstr(location_start, "\r\n", location_start - buffer);
			if (location_end != NULL) {
				len = location_end - location_start;
				if (len + 1 > LOCATION_SIZE) {
					cc_log_error("Buffer to small");
					return CC_RESULT_FAIL;
				}
				strncpy(location, location_start, len);
				location[len] = '\0';
				return CC_RESULT_SUCCESS;
			} else
				cc_log_error("Failed to get end of 'LOCATION'");
		} else
			cc_log_error("No attribute 'LOCATION'");
	}

	cc_log_error("No ssdp response");

	return CC_RESULT_FAIL;
}

static result_t transport_socket_get_ip_uri(const char *address, int port, const char *url, char *uri)
{
	struct sockaddr_in server;
	int fd, len;
	char buffer[RECV_BUF_SIZE];
	char *uri_start = NULL, *uri_end = NULL;

	len = snprintf(buffer, RECV_BUF_SIZE, "GET /%s HTTP/1.0\r\n\r\n", url);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		cc_log_error("Failed to create socket");
		return CC_RESULT_FAIL;
	}

	server.sin_addr.s_addr = inet_addr(address);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		cc_log_error("Failed to connect socket");
		return CC_RESULT_FAIL;
	}

	if (send(fd, buffer, len, 0) < 0) {
		cc_log_error("Failed to send data");
		return CC_RESULT_FAIL;
	}

	memset(&buffer, 0, RECV_BUF_SIZE);
	len = recv(fd, buffer, RECV_BUF_SIZE, 0);
	close(fd);

	if (len < 0) {
		cc_log_error("Failed read from socket");
		return CC_RESULT_FAIL;
	}

	buffer[len] = '\0';

	if (strncmp(buffer, "HTTP/1.0 200 OK", 15) != 0) {
		cc_log_error("Bad response");
		return CC_RESULT_FAIL;
	}

	uri_start = strnstr(buffer, "calvinip://", len);
	if (uri_start == NULL) {
		cc_log_error("No calvinip interface");
		return CC_RESULT_FAIL;
	}

	uri_end = strnstr(uri_start, "\"", uri_start - buffer);
	if (uri_end == NULL) {
		cc_log_error("Failed to parse interface");
		return CC_RESULT_FAIL;
	}

	strncpy(uri, uri_start, uri_end - uri_start);
	uri[uri_end - uri_start] = '\0';

	return CC_RESULT_SUCCESS;
}

static result_t transport_socket_discover_proxy(char *uri)
{
	int control_port;
	char location[LOCATION_SIZE];
	char address[ADDRESS_SIZE];
	char url[URL_SIZE];

	if (transport_socket_discover_location(location) == CC_RESULT_SUCCESS) {
		if (sscanf(location, "http://%99[^:]:%99d/%99[^\n]", address, &control_port, url) == 3)
			return transport_socket_get_ip_uri(address, control_port, url, uri);
	}

	return CC_RESULT_FAIL;
}
#endif

static int transport_socket_send(transport_client_t *transport_client, char *data, size_t size)
{
	return write(((transport_socket_client_t *)transport_client->client_state)->fd, data, size);
}

static int transport_socket_recv(transport_client_t *transport_client, char *buffer, size_t size)
{
	return read(((transport_socket_client_t *)transport_client->client_state)->fd, buffer, size);
}

static void transport_socket_disconnect(node_t *node, transport_client_t *transport_client)
{
	if (transport_client->state != TRANSPORT_DISCONNECTED) {
		close(((transport_socket_client_t *)transport_client)->fd);
		transport_client->state = TRANSPORT_DISCONNECTED;
	}
}

static void transport_socket_free(transport_client_t *transport_client)
{
	if (transport_client->client_state != NULL)
		platform_mem_free((void *)transport_client->client_state);
	platform_mem_free((void *)transport_client);
}

static result_t transport_socket_connect(node_t *node, transport_client_t *transport_client)
{
	transport_socket_client_t *transport_socket = (transport_socket_client_t *)transport_client->client_state;
	struct sockaddr_in server;

	transport_socket->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (transport_socket->fd < 0) {
		cc_log_error("Failed to create socket");
		return CC_RESULT_FAIL;
	}

	server.sin_addr.s_addr = inet_addr(transport_socket->ip);
	server.sin_port = htons(transport_socket->port);
	server.sin_family = AF_INET;

	if (connect(transport_socket->fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		cc_log_error("Failed to connect socket");
		return CC_RESULT_FAIL;
	}

	transport_client->state = TRANSPORT_CONNECTED;

	return CC_RESULT_SUCCESS;
}

transport_client_t *transport_socket_create(node_t *node, char *uri)
{
#ifdef CC_TRANSPORT_SOCKET_SSDP_ENABLED
	char discovery_result[100];
#endif
	transport_client_t *transport_client = NULL;
	transport_socket_client_t *transport_socket = NULL;

	if (platform_mem_alloc((void **)&transport_client, sizeof(transport_client_t)) == CC_RESULT_SUCCESS) {
		memset(transport_client, 0, sizeof(transport_client_t));
		transport_client->transport_type = TRANSPORT_SOCKET_TYPE;
		transport_client->state = TRANSPORT_INTERFACE_UP;
		transport_client->rx_buffer.buffer = NULL;
		transport_client->rx_buffer.pos = 0;
		transport_client->rx_buffer.size = 0;
		transport_client->connect = transport_socket_connect;
		transport_client->send = transport_socket_send;
		transport_client->recv = transport_socket_recv;
		transport_client->disconnect = transport_socket_disconnect;
		transport_client->free = transport_socket_free;
		transport_client->prefix_len = TRANSPORT_LEN_PREFIX_SIZE;

		if (platform_mem_alloc((void **)&transport_socket, sizeof(transport_socket_client_t)) == CC_RESULT_SUCCESS) {
			transport_client->client_state = transport_socket;
#ifdef CC_TRANSPORT_SOCKET_SSDP_ENABLED
			if (strncmp(uri, "ssdp", 4) == 0) {
				if (transport_socket_discover_proxy(discovery_result) == CC_RESULT_SUCCESS) {
					if (sscanf(discovery_result, "calvinip://%99[^:]:%99d", transport_socket->ip, &transport_socket->port) == 2) {
						cc_log("SSDP response: ip '%s' port '%d'", transport_socket->ip, transport_socket->port);
						strncpy(transport_client->uri, discovery_result, MAX_URI_LEN);
						return transport_client;
					}
					cc_log_error("Failed to parse uri '%s'", discovery_result);
				}
			} else {
#endif
				if (sscanf(uri, "calvinip://%99[^:]:%99d", transport_socket->ip, &transport_socket->port) == 2) {
					strncpy(transport_client->uri, uri, MAX_URI_LEN);
					return transport_client;
				}
				cc_log_error("Failed to parse uri '%s'", uri);
#ifdef CC_TRANSPORT_SOCKET_SSDP_ENABLED
			}
#endif
			platform_mem_free((void *)transport_socket);
		} else
			cc_log_error("Failed to allocate memory");
		platform_mem_free((void *)transport_client);
	} else
		cc_log_error("Failed to allocate memory");

	return NULL;
}
