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

#define LOCATION_SIZE       100
#define URL_SIZE            100
#define URI_SIZE            100
#define ADDRESS_SIZE        20
#define IP_SIZE             20
#define SSDP_MULTICAST      "239.255.255.250"
#define SSDP_PORT           1900
#define SERVICE_UUID        "1693326a-abb9-11e4-8dfb-9cb654a16426"

static transport_client_t m_client;

static result_t transport_discover_location(const char *interface, char *location)
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

static result_t transport_get_ip_uri(const char *address, int port, const char *url, char *uri)
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

/**
 * Find a peer runtime by broadcasting on iface
 * @param  iface interface to broadcast on
 * @param  ip    returned ip
 * @param  port  returned port
 * @return       result SUCCESS/FAIL
 */
static result_t transport_discover_proxy(const char *iface, char *ip, int *port)
{
	int control_port;
	char location[LOCATION_SIZE];
	char address[ADDRESS_SIZE];
	char url[URL_SIZE];
	char uri[URI_SIZE];

	log_debug("Starting discovery on %s", iface);

	if (transport_discover_location(iface, location) == SUCCESS) {
		if (sscanf(location, "http://%99[^:]:%99d/%99[^\n]", address, &control_port, url) != 3) {
			log_error("Failed to parse discovery response");
			return FAIL;
		}

		if (transport_get_ip_uri(address, control_port, url, uri) == SUCCESS) {
			if (sscanf(uri, "calvinip://%99[^:]:%99d", ip, port) != 2) {
				log_error("Failed to parse discovery response");
				return FAIL;
			}

			return SUCCESS;
		}
	}

	return FAIL;
}

result_t transport_start(const char *ssdp_iface, const char *proxy_iface, const int proxy_port)
{
	struct sockaddr_in server;
	char ip[40];
	int port = 0;

	memset(ip, 0, 40);

	if (proxy_iface == NULL) {
		log("Starting discovery");
		if (transport_discover_proxy(ssdp_iface, ip, &port) != SUCCESS) {
			log_error("No proxy found");
			return FAIL;
		}
	} else {
		strncpy(ip, proxy_iface, strlen(proxy_iface));
		port = proxy_port;
	}

	m_client.fd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_client.fd < 0) {
		log_error("Failed to create socket");
		return FAIL;
	}

	m_client.state = TRANSPORT_DISCONNECTED;
	m_client.state = TRANSPORT_DISCONNECTED;
	m_client.rx_buffer.buffer = NULL;
	m_client.rx_buffer.pos = 0;
	m_client.rx_buffer.size = 0;
	m_client.tx_buffer.buffer = NULL;
	m_client.tx_buffer.pos = 0;
	m_client.tx_buffer.size = 0;

	server.sin_addr.s_addr = inet_addr(ip);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	log("Connecting to proxy '%s:%d'", ip, port);

	if (connect(m_client.fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		log_error("Failed to connect socket");
		return FAIL;
	}

	m_client.state = TRANSPORT_CONNECTED;
	node_transmit();

	return SUCCESS;
}

void transport_stop(void)
{
	m_client.state = TRANSPORT_DISCONNECTED;
}

static result_t transport_free_tx_buffer(void)
{
	platform_mem_free((void *)m_client.tx_buffer.buffer);
	m_client.tx_buffer.pos = 0;
	m_client.tx_buffer.size = 0;
	m_client.tx_buffer.buffer = NULL;
	return SUCCESS;
}

result_t transport_send(size_t len)
{
	m_client.tx_buffer.buffer[0] = len >> 24 & 0xFF;
	m_client.tx_buffer.buffer[1] = len >> 16 & 0xFF;
	m_client.tx_buffer.buffer[2] = len >> 8 & 0xFF;
	m_client.tx_buffer.buffer[3] = len & 0xFF;

	if (send(m_client.fd, m_client.tx_buffer.buffer, len + 4, 0) < 0) {
		log_error("Failed to send data");
		return FAIL;
	}

	transport_free_tx_buffer();

	log_debug("Sent %zu", len + 4);

	node_transmit();

	return SUCCESS;
}

result_t transport_select(uint32_t timeout)
{
	result_t result = SUCCESS;
	fd_set fd_set;
	int max_fd = 0, status = 0, read_pos = 0;
	size_t msg_size = 0;
	char buffer[BUFFER_SIZE];
	struct timeval tv = {timeout, 0};

	FD_ZERO(&fd_set);

	if (m_client.state != TRANSPORT_DISCONNECTED) {
		FD_SET(m_client.fd, &fd_set);
		if (m_client.fd > max_fd)
			max_fd = m_client.fd;
	}

	if (select(max_fd + 1, &fd_set, NULL, NULL, &tv) < 0) {
		log_error("ERROR on select");
		return FAIL;
	}

	memset(&buffer, 0, BUFFER_SIZE);

	if (m_client.state != TRANSPORT_DISCONNECTED) {
		status = recv(m_client.fd, buffer, BUFFER_SIZE, 0);
		if (status == 0) {
			// TODO: disconnect tunnels using this connection
			m_client.state = TRANSPORT_DISCONNECTED;
			log_error("Connection closed");
			return FAIL;
		}

		while (read_pos < status) {
			// New message?
			if (m_client.rx_buffer.buffer == NULL) {
				msg_size = get_message_len(buffer + read_pos);
				read_pos += 4;

				// Complete message?
				if (msg_size <= status - read_pos) {
					node_handle_data(buffer + read_pos, msg_size);
					read_pos += msg_size;
				} else {
					if (platform_mem_alloc((void **)&m_client.rx_buffer.buffer, msg_size) != SUCCESS) {
						log_error("Failed to allocate rx buffer");
						return FAIL;
					}
					memcpy(m_client.rx_buffer.buffer, buffer + read_pos, status - read_pos);
					m_client.rx_buffer.pos = status - read_pos;
					m_client.rx_buffer.size = msg_size;
					return SUCCESS;
				}
			} else {
				// Fragment completing message?
				if (m_client.rx_buffer.size - m_client.rx_buffer.pos <= status - read_pos) {
					memcpy(m_client.rx_buffer.buffer + m_client.rx_buffer.pos, buffer + read_pos, m_client.rx_buffer.size - m_client.rx_buffer.pos);
					node_handle_data(m_client.rx_buffer.buffer, m_client.rx_buffer.size);
					platform_mem_free((void *)m_client.rx_buffer.buffer);
					read_pos += m_client.rx_buffer.size - m_client.rx_buffer.pos;
					m_client.rx_buffer.buffer = NULL;
					m_client.rx_buffer.pos = 0;
					m_client.rx_buffer.size = 0;
				} else {
					memcpy(m_client.rx_buffer.buffer + m_client.rx_buffer.pos, buffer + read_pos, status - read_pos);
					m_client.rx_buffer.pos += status - read_pos;
					return SUCCESS;
				}
			}
		}
	}

	return result;
}

void transport_set_state(const transport_state_t state)
{
	m_client.state = state;
}

transport_state_t transport_get_state(void)
{
	return m_client.state;
}

bool transport_can_send(void)
{
	return m_client.tx_buffer.buffer == NULL;
}

result_t transport_get_tx_buffer(char **buffer, uint32_t size)
{
	if (m_client.tx_buffer.buffer != NULL) {
		log_error("TX buffer is not NULL");
		return FAIL;
	}

	if (platform_mem_alloc((void **)buffer, size) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	m_client.tx_buffer.pos = 0;
	m_client.tx_buffer.size = 0;
	m_client.tx_buffer.buffer = *buffer;
	return SUCCESS;
}

#ifdef LWM2M_HTTP_CLIENT
result_t transport_http_get(char *iface, int port, char *url, char *buffer, int buffer_size)
{
	struct sockaddr_in server;
	int fd, len;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		log_error("Failed to create socket");
		return FAIL;
	}

	server.sin_addr.s_addr = inet_addr(iface);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		log_error("Failed to connect socket");
		return FAIL;
	}

	len = sprintf(buffer, "GET %s HTTP/1.0\r\n\r\n", url);

	if (send(fd, buffer, len, 0) < 0) {
		log_error("Failed to send data");
		return FAIL;
	}

	memset(buffer, 0, buffer_size);
	len = recv(fd, buffer, buffer_size, 0);
	close(fd);

	if (len < 0) {
		log_error("Failed read from socket");
		return FAIL;
	}

	buffer[len] = '\0';

	return SUCCESS;
}
#endif
