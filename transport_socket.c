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
#include "transport.h"
#include "platform.h"
#include "node.h"

#define BUFFER_SIZE         2048
#define LOCATION_SIZE       100
#define URL_SIZE            100
#define URI_SIZE            100
#define ADDRESS_SIZE        20
#define IP_SIZE             20
#define SSDP_MULTICAST      "239.255.255.250"
#define SSDP_PORT           1900
#define SERVICE_UUID        "1693326a-abb9-11e4-8dfb-9cb654a16426"

static result_t discover_location(const char *interface, char *location)
{
	int sock;
	size_t ret;
	unsigned int socklen, len;
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
	sockname.sin_addr.s_addr = inet_addr(interface);

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
		if (len  == (size_t)-1) {
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

static result_t get_ip_uri(const char *address, int port, const char *url, char *uri)
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

	bzero(buffer, BUFFER_SIZE);
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

/**
 * Find a peer runtime by broadcasting on iface
 * @param  iface interface to broadcast on
 * @param  ip    returned ip
 * @param  port  returned port
 * @return       result SUCCESS/FAIL
 */
result_t discover_proxy(const char *iface, char *ip, int *port)
{
	int control_port;
	char location[LOCATION_SIZE];
	char address[ADDRESS_SIZE];
	char url[URL_SIZE];
	char uri[URI_SIZE];

	log_debug("Starting discovery on %s", iface);

	if (discover_location(iface, location) == SUCCESS) {
		if (sscanf(location, "http://%99[^:]:%99d/%99[^\n]", address, &control_port, url) != 3) {
			log_error("Failed to parse discovery response");
			return FAIL;
		}

		if (get_ip_uri(address, control_port, url, uri) == SUCCESS) {
			if (sscanf(uri, "calvinip://%99[^:]:%99d", ip, port) != 2) {
				log_error("Failed to parse discovery response");
				return FAIL;
			}

			return SUCCESS;
		}
	}

	return FAIL;
}

transport_client_t *client_connect(const char *address, int port)
{
	struct sockaddr_in server;
	transport_client_t *client = NULL;

	client = (transport_client_t *)malloc(sizeof(transport_client_t));
	if (client == NULL) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	client->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client->fd < 0) {
		log_error("Failed to create socket");
		free(client);
		return NULL;
	}

	client->state = TRANSPORT_CONNECTED;
	client->buffer = NULL;
	client->buffer_pos = 0;
	client->msg_size = 0;

	server.sin_addr.s_addr = inet_addr(address);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	if (connect(client->fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		log_error("Failed to connect socket");
		free(client);
		return NULL;
	}

	log_debug("Connected to %s:%d", address, port);

	return client;
}

result_t client_send(const transport_client_t *client, char *data, size_t len)
{
	result_t result = SUCCESS;

	data[0] = (len & 0xFF000000);
	data[1] = (len & 0x00FF0000);
	data[2] = (len & 0x0000FF00) / 0x000000FF;
	data[3] = (len & 0x000000FF);

	if (send(client->fd, data, len + 4, 0) < 0) {
		result = FAIL;
		log_error("Failed to send data");
	}

	free(data);

	return result;
}

result_t wait_for_data(transport_client_t **client, uint32_t timeout)
{
	result_t result = SUCCESS;
	fd_set fd_set;
	int max_fd = 0, status = 0, read_pos = 0, msg_size = 0;
	char buffer[BUFFER_SIZE];
	struct timeval tv = {timeout, 0};

	FD_ZERO(&fd_set);

	// Add stdin
	FD_SET(0, &fd_set);

	// proxy socket
	if (*client != NULL && (*client)->state != TRANSPORT_DISCONNECTED) {
		FD_SET((*client)->fd, &fd_set);
		if ((*client)->fd > max_fd)
			max_fd = (*client)->fd;
	}

	if (select(max_fd + 1, &fd_set, NULL, NULL, &tv) < 0) {
		log_error("ERROR on select");
		return FAIL;
	}

	bzero(buffer, BUFFER_SIZE);

	// Stop node on any data
	if (FD_ISSET(0, &fd_set))
		stop_node(true);

	if (*client != NULL && FD_ISSET((*client)->fd, &fd_set)) {
		status = recv((*client)->fd, buffer, BUFFER_SIZE, 0);
		if (status == 0) {
			// TODO: disconnect tunnels using this connection
			(*client)->state = TRANSPORT_DISCONNECTED;
			log_debug("Connection closed");
			return FAIL;
		}
		while (read_pos < status) {
			if ((*client)->buffer == NULL) {
				msg_size = get_message_len(buffer + read_pos);
				read_pos += 4; // Skip header
				if (msg_size <= status - read_pos) {
					handle_data(buffer + read_pos, msg_size);
					read_pos += msg_size;
				} else {
					(*client)->buffer = malloc(msg_size);
					if ((*client)->buffer == NULL) {
						log_error("Failed to allocate memory");
						return FAIL;
					}
					memcpy((*client)->buffer, buffer + read_pos, status - read_pos);
					(*client)->msg_size = msg_size;
					(*client)->buffer_pos = status - read_pos;
					return SUCCESS;
				}
			} else {
				msg_size = (*client)->msg_size;
				if (msg_size - (*client)->buffer_pos <= status - read_pos) {
					memcpy((*client)->buffer + (*client)->buffer_pos, buffer + read_pos, msg_size - (*client)->buffer_pos);
					handle_data((*client)->buffer, msg_size);
					free((*client)->buffer);
					(*client)->buffer = NULL;
					read_pos += msg_size - (*client)->buffer_pos;
					(*client)->buffer = 0;
					(*client)->msg_size = 0;
					(*client)->buffer_pos = 0;
				} else {
					memcpy((*client)->buffer + (*client)->buffer_pos, buffer + read_pos, status - read_pos);
					(*client)->buffer_pos += status - read_pos;
					return SUCCESS;
				}
			}
		}
	}

	return result;
}

void free_client(transport_client_t *client)
{
	free(client);
}
