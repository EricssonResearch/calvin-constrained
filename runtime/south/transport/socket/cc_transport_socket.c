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
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#ifdef CC_PLATFORM_ANDROID
#include <sys/socket.h>
#endif
#include <string.h>
#include "cc_transport_socket.h"
#include "runtime/north/cc_transport.h"
#include "runtime/south/platform/cc_platform.h"
#include "runtime/north/cc_node.h"

#define CC_LOCATION_SIZE	200
#define CC_URL_SIZE	100
#define CC_ADDRESS_SIZE	20
#define CC_SSDP_MULTICAST	"239.255.255.250"
#define CC_SSDP_PORT	1900
#define CC_SERVICE_UUID	"1693326a-abb9-11e4-8dfb-9cb654a16426"
#define CC_RECV_BUF_SIZE	512

static cc_result_t cc_transport_socket_discover_location(char *location)
{
	int sock = 0;
	unsigned int socklen = 0;
	int len = 0, ret = 0;
	struct sockaddr_in sockname;
	struct sockaddr clientsock;
	char buffer[CC_RECV_BUF_SIZE];
	fd_set fds;
	char *location_start = NULL;
	char *location_end = NULL;
	struct timeval tv = {5, 0};

	len = snprintf(buffer, CC_RECV_BUF_SIZE, "M-SEARCH * HTTP/1.1\r\n" \
		"HOST: %s:%d\r\n" \
		"MAN: \"ssdp:discover\"\r\n" \
		"MX: 2\r\n" \
		"ST: uuid:%s\r\n" \
		"\r\n",
		CC_SSDP_MULTICAST, CC_SSDP_PORT, CC_SERVICE_UUID);

	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		cc_log_error("Failed to send ssdp request");
		return CC_FAIL;
	}

	memset((char *)&sockname, 0, sizeof(struct sockaddr_in));
	sockname.sin_family = AF_INET;
	sockname.sin_port = htons(CC_SSDP_PORT);
	sockname.sin_addr.s_addr = inet_addr(CC_SSDP_MULTICAST);

	cc_log("transport_socket: Sending ssdp request");

	ret = sendto(sock, buffer, len, 0, (struct sockaddr *) &sockname, sizeof(struct sockaddr_in));
	if (ret != len) {
		cc_log_error("Failed to send ssdp request");
		return CC_FAIL;
	}

	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	if (select(sock + 1, &fds, NULL, NULL, &tv) <= 0) {
		cc_log_debug("No ssdp response");
		return CC_FAIL;
	}

	if (FD_ISSET(sock, &fds)) {
		socklen = sizeof(clientsock);
		len = recvfrom(sock, buffer, CC_RECV_BUF_SIZE, MSG_PEEK, &clientsock, &socklen);
		if (len == -1) {
			cc_log_error("Failed to receive ssdp response");
			return CC_FAIL;
		}

		buffer[len] = '\0';
		close(sock);

		if (strncmp(buffer, "HTTP/1.1 200 OK", 15) != 0) {
			cc_log_error("Bad ssdp response");
			return CC_FAIL;
		}

		location_start = strstr(buffer, "LOCATION:");
		if (location_start != NULL) {
			location_start = location_start + 10;
			location_end = strstr(location_start, "\r\n");
			if (location_end != NULL) {
				len = location_end - location_start;
				if (len + 1 > CC_LOCATION_SIZE) {
					cc_log_error("Buffer to small");
					return CC_FAIL;
				}
				strncpy(location, location_start, len);
				location[len] = '\0';
				return CC_SUCCESS;
			} else
				cc_log_error("Failed to get end of 'LOCATION'");
		} else
			cc_log_error("No attribute 'LOCATION'");
	}

	cc_log_error("No ssdp response");

	return CC_FAIL;
}

static cc_result_t cc_transport_socket_get_ip_uri(char *uri, char *ip, int ip_len, int port, char *path)
{
	struct sockaddr_in server;
	int fd, len;
	char buffer[CC_RECV_BUF_SIZE];
	char *uri_start = NULL, *uri_end = NULL;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		cc_log_error("Failed to create socket");
		return CC_FAIL;
	}

	strncpy(buffer, ip, ip_len);
	buffer[ip_len] = '\0';
	server.sin_addr.s_addr = inet_addr(buffer);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		cc_log_error("Failed to connect socket");
		return CC_FAIL;
	}

	memset(buffer, 0, CC_RECV_BUF_SIZE);
	len = snprintf(buffer, CC_RECV_BUF_SIZE, "GET %s HTTP/1.0\r\n\r\n", path);

	if (send(fd, buffer, len, 0) < 0) {
		cc_log_error("Failed to send data");
		return CC_FAIL;
	}

	memset(buffer, 0, CC_RECV_BUF_SIZE);
	len = recv(fd, buffer, CC_RECV_BUF_SIZE, 0);
	close(fd);

	if (len < 0) {
		cc_log_error("Failed read from socket");
		return CC_FAIL;
	}

	buffer[len] = '\0';

	if (strncmp(buffer, "HTTP/1.0 200 OK", 15) != 0) {
		cc_log_error("Bad response '%s'", buffer);
		return CC_FAIL;
	}

	uri_start = strstr(buffer, "calvinip://");
	if (uri_start == NULL) {
		cc_log_error("No calvinip interface");
		return CC_FAIL;
	}

	uri_end = strchr(uri_start, '"');
	if (uri_end == NULL) {
		cc_log_error("Failed to parse interface");
		return CC_FAIL;
	}

	strncpy(uri, uri_start, uri_end - uri_start);
	uri[uri_end - uri_start] = '\0';

	return CC_SUCCESS;
}

static cc_result_t cc_transport_socket_discover_proxy(char *uri)
{
	int pos = 7, location_len = 0, ip_len = 0, port = 0;
	char location[CC_LOCATION_SIZE];
	char *ip = NULL, *path_start = NULL, *end = NULL;

	memset(location, 0, CC_LOCATION_SIZE);

	if (cc_transport_socket_discover_location(location) != CC_SUCCESS)
		return CC_FAIL;

	location_len = strnlen(location, CC_LOCATION_SIZE);

	if (strncmp(location, "http://", 7) != 0) {
		cc_log_error("Failed to parse ssdp response");
		return CC_FAIL;
	}

	while (pos < location_len) {
		if (location[pos] == ':') {
			ip = location + 7;
			ip_len = pos - 7;
			break;
		}
		pos++;
	}

	if (ip == NULL) {
		cc_log_error("Failed to parse ssdp response");
		return CC_FAIL;
	}

	path_start = strchr(ip, '/');
	if (path_start == NULL) {
		cc_log_error("Failed to parse ssdp response");
		return CC_FAIL;
	}

	port = strtol(ip + ip_len + 1, &end, 10);
	if (end == ip + ip_len + 1) {
		cc_log_error("Failed to parse ssdp response");
		return CC_FAIL;
	}

	return cc_transport_socket_get_ip_uri(uri, ip, ip_len, port, path_start);
}

static int cc_transport_socket_send(cc_transport_client_t *transport_client, char *data, size_t size)
{
	return write(((cc_transport_socket_client_t *)transport_client->client_state)->fd, data, size);
}

static int cc_transport_socket_recv(cc_transport_client_t *transport_client, char *buffer, size_t size)
{
	return read(((cc_transport_socket_client_t *)transport_client->client_state)->fd, buffer, size);
}

static void cc_transport_socket_disconnect(cc_node_t *node, cc_transport_client_t *transport_client)
{
	if (transport_client->state != CC_TRANSPORT_DISCONNECTED)
		close(((cc_transport_socket_client_t *)transport_client)->fd);
}

static void cc_transport_socket_free(cc_transport_client_t *transport_client)
{
	if (transport_client->client_state != NULL)
		cc_platform_mem_free((void *)transport_client->client_state);
	cc_platform_mem_free((void *)transport_client);
}

static cc_result_t cc_transport_socket_connect(cc_node_t *node, cc_transport_client_t *transport_client)
{
	cc_transport_socket_client_t *transport_socket = (cc_transport_socket_client_t *)transport_client->client_state;
	struct sockaddr_in server;

	transport_socket->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (transport_socket->fd < 0) {
		cc_log_error("Failed to create socket");
		return CC_FAIL;
	}

	server.sin_addr.s_addr = inet_addr(transport_socket->ip);
	server.sin_port = htons(transport_socket->port);
	server.sin_family = AF_INET;

	if (connect(transport_socket->fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		cc_log_error("Failed to connect socket");
		return CC_FAIL;
	}

	transport_client->state = CC_TRANSPORT_CONNECTED;

	return CC_SUCCESS;
}

static cc_result_t cc_transport_socket_parse_uri(char *uri, char **ip, size_t *ip_len, int *port)
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

cc_transport_client_t *cc_transport_socket_create(cc_node_t *node, char *uri)
{
	char ssdpuri[100];
	cc_transport_client_t *transport_client = NULL;
	cc_transport_socket_client_t *transport_socket = NULL;
	char *ip = NULL;
	int port = 0;
	size_t ip_len = 0;

	if (strncmp(uri, "ssdp", 4) == 0) {
		if (cc_transport_socket_discover_proxy(ssdpuri) != CC_SUCCESS) {
			cc_log_error("Failed to parse ssdp response");
			return NULL;
		}

		if (cc_transport_socket_parse_uri(ssdpuri, &ip, &ip_len, &port) != CC_SUCCESS) {
			cc_log_error("Failed to parse uri '%s'", uri);
			return NULL;
		}
	} else {
		if (cc_transport_socket_parse_uri(uri, &ip, &ip_len, &port) != CC_SUCCESS) {
			cc_log_error("Failed to parse uri '%s'", uri);
			return NULL;
		}
	}

	if (cc_platform_mem_alloc((void **)&transport_client, sizeof(cc_transport_client_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	if (cc_platform_mem_alloc((void **)&transport_socket, sizeof(cc_transport_socket_client_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	memset(transport_client, 0, sizeof(cc_transport_client_t));
	transport_client->transport_type = CC_TRANSPORT_SOCKET_TYPE;
	transport_client->state = CC_TRANSPORT_INTERFACE_UP;
	transport_client->rx_buffer.buffer = NULL;
	transport_client->rx_buffer.pos = 0;
	transport_client->rx_buffer.size = 0;
	transport_client->connect = cc_transport_socket_connect;
	transport_client->send = cc_transport_socket_send;
	transport_client->recv = cc_transport_socket_recv;
	transport_client->disconnect = cc_transport_socket_disconnect;
	transport_client->free = cc_transport_socket_free;
	transport_client->prefix_len = CC_TRANSPORT_LEN_PREFIX_SIZE;
	strncpy(transport_socket->ip, ip, ip_len);
	transport_socket->ip[ip_len] = '\0';
	transport_socket->port = port;
	sprintf(transport_client->uri, "calvinip://%s:%d", transport_socket->ip, transport_socket->port);
	transport_client->client_state = transport_socket;

	return transport_client;
}
