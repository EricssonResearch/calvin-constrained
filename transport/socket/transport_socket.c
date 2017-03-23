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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "../../transport.h"
#include "../../platform.h"
#include "../../node.h"
#include "transport_socket.h"

#define LOCATION_SIZE       100
#define URL_SIZE            100
#define URI_SIZE            100
#define ADDRESS_SIZE        20
#define IP_SIZE             20
#define SSDP_MULTICAST      "239.255.255.250"
#define SSDP_PORT           1900
#define SERVICE_UUID        "1693326a-abb9-11e4-8dfb-9cb654a16426"

static result_t transport_socket_discover_location(char *location)
{
	int sock;
	size_t ret;
	unsigned int socklen;
	int len;
	struct sockaddr_in sockname;
	struct sockaddr clientsock;
	char buffer[BUFFER_SIZE];
	fd_set fds;
	char *location_start = NULL;
	char *location_end = NULL;
	struct timeval tv = {5, 0};

	sprintf(buffer, "M-SEARCH * HTTP/1.1\r\n" \
		"HOST: %s:%d\r\n" \
		"MAN: \"ssdp:discover\"\r\n" \
		"MX: 2\r\n" \
		"ST: uuid:%s\r\n" \
		"\r\n",
		SSDP_MULTICAST, SSDP_PORT, SERVICE_UUID);

	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		log_error("Failed to send ssdp request");
		return FAIL;
	}

	memset((char *)&sockname, 0, sizeof(struct sockaddr_in));
	sockname.sin_family = AF_INET;
	sockname.sin_port = htons(SSDP_PORT);
	sockname.sin_addr.s_addr = inet_addr(SSDP_MULTICAST);

	log("Sending ssdp request");

	ret = sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *) &sockname, sizeof(struct sockaddr_in));
	if (ret != strlen(buffer)) {
		log_error("Failed to send ssdp request");
		return FAIL;
	}

	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	if (select(sock + 1, &fds, NULL, NULL, &tv) < 0) {
		log_error("Failed to receive ssdp response");
		return FAIL;
	}

	if (FD_ISSET(sock, &fds)) {
		socklen = sizeof(clientsock);
		len = recvfrom(sock, buffer, BUFFER_SIZE, MSG_PEEK, &clientsock, &socklen);
		if (len == -1) {
			log_error("Failed to receive ssdp response");
			return FAIL;
		}

		buffer[len] = '\0';
		close(sock);

		if (strncmp(buffer, "HTTP/1.1 200 OK", 12) != 0) {
			log_error("Bad ssdp response");
			return FAIL;
		}

		location_start = strstr(buffer, "LOCATION:");
		if (location_start != NULL) {
			location_end = strstr(location_start, "\r\n");
			if (location_end != NULL) {
				if (strncpy(location, location_start + 10, location_end - (location_start + 10)) != NULL) {
					location[location_end - (location_start + 10)] = '\0';
					return SUCCESS;
				}
				log_error("Failed to allocate memory");
			} else
				log_error("Failed to parse ssdp response");
		} else
			log_error("Failed to parse ssdp response");
	} else
		log_error("No ssdp response");

	return FAIL;
}

static result_t transport_socket_get_ip_uri(const char *address, int port, const char *url, char *uri)
{
	struct sockaddr_in server;
	int fd, len;
	char buffer[BUFFER_SIZE];
	char *uri_start = NULL, *uri_end = NULL;

	sprintf(buffer, "GET /%s HTTP/1.0\r\n\r\n", url);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		log_error("Failed to create socket");
		return FAIL;
	}

	server.sin_addr.s_addr = inet_addr(address);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		log_error("Failed to connect socket");
		return FAIL;
	}

	if (send(fd, buffer, strlen(buffer), 0) < 0) {
		log_error("Failed to send data");
		return FAIL;
	}

	memset(&buffer, 0, BUFFER_SIZE);
	len = recv(fd, buffer, BUFFER_SIZE, 0);
	close(fd);

	if (len < 0) {
		log_error("Failed read from socket");
		return FAIL;
	}

	buffer[len] = '\0';

	if (strncmp(buffer, "HTTP/1.0 200 OK", 12) != 0) {
		log_error("Bad response");
		return FAIL;
	}

	uri_start = strstr(buffer, "calvinip://");
	if (uri_start == NULL) {
		log_error("No calvinip interface");
		return FAIL;
	}

	uri_end = strstr(uri_start, "\"");
	if (uri_end == NULL) {
		log_error("Failed to parse interface");
		return FAIL;
	}

	strncpy(uri, uri_start, uri_end - uri_start);
	uri[uri_end - uri_start] = '\0';

	return SUCCESS;
}

static result_t transport_socket_discover_proxy(char *uri)
{
	int control_port;
	char location[LOCATION_SIZE];
	char address[ADDRESS_SIZE];
	char url[URL_SIZE];

	if (transport_socket_discover_location(location) == SUCCESS) {
		if (sscanf(location, "http://%99[^:]:%99d/%99[^\n]", address, &control_port, url) != 3) {
			log_error("Failed to parse discovery response");
			return FAIL;
		}

		return transport_socket_get_ip_uri(address, control_port, url, uri);
	}

	return FAIL;
}

static result_t transport_socket_send_tx_buffer(const node_t *node, transport_client_t *transport_client, size_t size)
{
	transport_append_buffer_prefix(transport_client->tx_buffer.buffer, size);

	if (send(((transport_socket_client_t *)transport_client->client_state)->fd, transport_client->tx_buffer.buffer, size + 4, 0) < 0) {
		log_error("Failed to send data");
		return FAIL;
	}

	transport_free_tx_buffer(transport_client);

	return SUCCESS;
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
		log_error("Failed to create socket");
		return FAIL;
	}

	server.sin_addr.s_addr = inet_addr(transport_socket->ip);
	server.sin_port = htons(transport_socket->port);
	server.sin_family = AF_INET;

	if (connect(transport_socket->fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		log_error("Failed to connect socket");
		return FAIL;
	}

	transport_join(node, transport_client);

	return SUCCESS;
}

transport_client_t *transport_socket_create(node_t *node, char *uri)
{
	char discovery_result[100];
	transport_client_t *transport_client = NULL;
	transport_socket_client_t *transport_socket = NULL;

	if (platform_mem_alloc((void **)&transport_client, sizeof(transport_client_t)) == SUCCESS) {
		transport_client->state = TRANSPORT_INTERFACE_UP;
		transport_client->rx_buffer.buffer = NULL;
		transport_client->rx_buffer.pos = 0;
		transport_client->rx_buffer.size = 0;
		transport_client->tx_buffer.buffer = NULL;
		transport_client->tx_buffer.pos = 0;
		transport_client->tx_buffer.size = 0;
		transport_client->connect = transport_socket_connect;
		transport_client->send_tx_buffer = transport_socket_send_tx_buffer;
		transport_client->disconnect = transport_socket_disconnect;
		transport_client->free = transport_socket_free;

		if (platform_mem_alloc((void **)&transport_socket, sizeof(transport_socket_client_t)) == SUCCESS) {
			transport_client->client_state = transport_socket;
			if (strncmp(uri, "ssdp", 4) == 0) {
				if (transport_socket_discover_proxy(discovery_result) == SUCCESS) {
					log("Discovery response '%s'", discovery_result);
					if (sscanf(discovery_result, "calvinip://%99[^:]:%99d", transport_socket->ip, &transport_socket->port) == 2)
						return transport_client;
					else
						log_error("Failed to parse uri '%s'", discovery_result);
				} else
					log_error("Discovery failed");
			} else {
				if (sscanf(uri, "calvinip://%99[^:]:%99d", transport_socket->ip, &transport_socket->port) == 2)
					return transport_client;
				else
					log_error("Failed to parse uri '%s'", uri);
			}
			platform_mem_free((void *)transport_socket);
		} else
			log_error("Failed to allocate memory");
		platform_mem_free((void *)transport_client);
	} else
		log_error("Failed to allocate memory");

	return NULL;
}
