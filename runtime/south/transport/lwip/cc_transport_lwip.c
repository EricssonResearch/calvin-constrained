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
#include <stdlib.h>
#include "lwip/inet6.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/netif.h"
#include "cc_transport_lwip.h"
#include "runtime/north/cc_transport.h"
#include "runtime/south/platform/cc_platform.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/cc_common.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static cc_transport_client_t m_transport_client;

static cc_result_t cc_transport_lwip_convert_mac_to_link_local(const char *mac, char *ip)
{
	long col1, col2, col3, col4, col5, col6;

	col1 = (unsigned char)strtol(mac, NULL, 16);
	col1 ^= 1 << 1;
	col2 = (unsigned char)strtol(mac + 3, NULL, 16);
	col3 = (unsigned char)strtol(mac + 6, NULL, 16);
	col4 = (unsigned char)strtol(mac + 9, NULL, 16);
	col5 = (unsigned char)strtol(mac + 12, NULL, 16);
	col6 = (unsigned char)strtol(mac + 15, NULL, 16);

	sprintf(ip,
		"fe80::%02lx%02lx:%02lxff:fe%02lx:%02lx%02lx",
		col1, col2, col3, col4, col5, col6);

	return CC_SUCCESS;
}

static void cc_transport_lwip_error_handler(void *arg, err_t err)
{
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(err);

	cc_log_error("Error on TCP port, reason 0x%08x", err);
}

static err_t cc_transport_lwip_connection_poll(void *arg, struct tcp_pcb *pcb)
{
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(pcb);

	return ERR_OK;
}

static void cc_transport_lwip_wait_for_state(cc_transport_client_t *client, cc_transport_state_t state)
{
	while (client->state != state)
		cc_platform_evt_wait(NULL, NULL);
}

static err_t cc_transport_lwip_write_complete(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	UNUSED_PARAMETER(pcb);
	cc_transport_client_t *transport_client = (cc_transport_client_t *)arg;

	transport_client->state = CC_TRANSPORT_ENABLED;

	return ERR_OK;
}

static int cc_transport_lwip_send(cc_transport_client_t *transport_client, char *buffer, size_t size)
{
	err_t res = ERR_BUF;
	uint32_t tcp_buffer_size = 0;
	cc_transport_lwip_client_t *transport_lwip = (cc_transport_lwip_client_t *)transport_client->client_state;
	struct tcp_pcb *pcb = transport_lwip->tcp_port;
	int written = 0, to_write = 0;

	while (written < size) {
		to_write = MIN(tcp_sndbuf(pcb), size - written);
		tcp_sent(pcb, cc_transport_lwip_write_complete);
		transport_client->state = CC_TRANSPORT_PENDING;
		res = tcp_write(pcb, buffer + written, to_write, 1);
		if (res != ERR_OK) {
			transport_client->state = CC_TRANSPORT_ENABLED;
			cc_log_error("Failed to write data");
			return -1;
		}
		cc_transport_lwip_wait_for_state(transport_client, CC_TRANSPORT_ENABLED);
		written += to_write;
	}

	return written;
}

static err_t cc_transport_lwip_data_handler(void *arg, struct tcp_pcb *pcb, struct pbuf *buffer, err_t err)
{
	cc_transport_client_t *transport_client = (cc_transport_client_t *)arg;
	cc_transport_lwip_client_t *state = (cc_transport_lwip_client_t *)transport_client->client_state;

	if (err == ERR_OK) {
		if (buffer->tot_len > CC_TRANSPORT_RX_BUFFER_SIZE - state->rx_buffer.size) {
			cc_log_error("TODO: Increase rx buffer or link rx buffers (read: %d rx_buffer: %d rx_buffer pos: %d)",
				buffer->tot_len,
				CC_TRANSPORT_RX_BUFFER_SIZE,
				state->rx_buffer.size);
		} else {
			memcpy(state->rx_buffer.buffer + state->rx_buffer.size, buffer->payload, buffer->tot_len);
			state->rx_buffer.size += buffer->tot_len;
		}

		tcp_recved(pcb, buffer->tot_len);
		UNUSED_VARIABLE(pbuf_free(buffer));
	}

	return err;
}

static void cc_transport_lwip_wait_for_data(cc_transport_client_t *transport_client)
{
	while (!cc_transport_lwip_has_data(transport_client))
		cc_platform_evt_wait(NULL, NULL);
}

static int cc_transport_lwip_recv(cc_transport_client_t *transport_client, char *buffer, size_t size)
{
	int ret = 0;
	cc_transport_lwip_client_t *state = (cc_transport_lwip_client_t *)transport_client->client_state;

	cc_transport_lwip_wait_for_data(transport_client);

	if (size < state->rx_buffer.size) {
		cc_log_error("Read buffer '%d' < read data '%d'", size, state->rx_buffer.size);
		return -1;
	}

	memcpy(buffer, state->rx_buffer.buffer, state->rx_buffer.size);
	ret = state->rx_buffer.size;
	state->rx_buffer.size = 0;

	return ret;
}

static err_t cc_transport_lwip_connection_callback(void *arg, struct tcp_pcb *pcb, err_t err)
{
	UNUSED_PARAMETER(pcb);
	cc_transport_client_t *transport_client = (cc_transport_client_t *)arg;

	if (err != ERR_OK) {
		cc_log_error("Failed to create TCP connection");
		transport_client->state = CC_TRANSPORT_INTERFACE_DOWN;
		return err;
	}

	transport_client->state = CC_TRANSPORT_CONNECTED;

	return ERR_OK;
}

static cc_result_t cc_transport_lwip_connect(cc_node_t *node, cc_transport_client_t *transport_client)
{
	cc_transport_lwip_client_t *transport_lwip = (cc_transport_lwip_client_t *)transport_client->client_state;
	ip6_addr_t ipv6_addr;
	err_t err;
	char ip[40];

	if (cc_transport_lwip_convert_mac_to_link_local(transport_lwip->mac, ip) != CC_SUCCESS) {
		cc_log_error("Failed to convert MAC address '%s'", transport_lwip->mac);
		return CC_FAIL;
	}

	if (!ip6addr_aton(ip, &ipv6_addr)) {
		cc_log_error("Failed to convert IP address");
		return CC_FAIL;
	}

	err = tcp_connect_ip6(transport_lwip->tcp_port, &ipv6_addr, 5000, cc_transport_lwip_connection_callback);
	if (err != ERR_OK) {
		cc_log_error("Failed to connect socket");
		return CC_FAIL;
	}

	cc_log("TCP connection requested to %s:%d.", ip, 5000);
	transport_client->state = CC_TRANSPORT_PENDING;
	cc_transport_lwip_wait_for_state(transport_client, CC_TRANSPORT_CONNECTED);
	cc_log("TCP connected to %s:%d.", ip, 5000);

	return CC_SUCCESS;
}

static void cc_transport_lwip_disconnect(cc_node_t *node, cc_transport_client_t *transport_client)
{
	cc_transport_lwip_client_t *transport_lwip = (cc_transport_lwip_client_t *)transport_client->client_state;

	tcp_close(transport_lwip->tcp_port);
	transport_client->state = CC_TRANSPORT_DISCONNECTED;
}

static void cc_transport_lwip_free(cc_transport_client_t *transport_client)
{
	if (transport_client->client_state != NULL)
		cc_platform_mem_free((void *)transport_client->client_state);
}

cc_transport_client_t *cc_transport_lwip_create(cc_node_t *node, char *uri)
{
	cc_transport_lwip_client_t *transport_lwip = NULL;

	if (cc_platform_mem_alloc(&transport_lwip, sizeof(cc_transport_lwip_client_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	memset(transport_lwip, 0, sizeof(cc_transport_lwip_client_t));
	transport_lwip->node = node;
	transport_lwip->tcp_port = tcp_new_ip6();
	transport_lwip->rx_buffer.size = 0;
	tcp_setprio(transport_lwip->tcp_port, TCP_PRIO_MIN);
	tcp_arg(transport_lwip->tcp_port, &m_transport_client);
	tcp_recv(transport_lwip->tcp_port, cc_transport_lwip_data_handler);
	tcp_err(transport_lwip->tcp_port, cc_transport_lwip_error_handler);
	tcp_poll(transport_lwip->tcp_port, cc_transport_lwip_connection_poll, 0);
	m_transport_client.client_state = transport_lwip;
	m_transport_client.state = CC_TRANSPORT_INTERFACE_DOWN;
	m_transport_client.rx_buffer.buffer = NULL;
	m_transport_client.rx_buffer.pos = 0;
	m_transport_client.rx_buffer.size = 0;
	m_transport_client.prefix_len = CC_TRANSPORT_LEN_PREFIX_SIZE;
	m_transport_client.connect = cc_transport_lwip_connect;
	m_transport_client.send = cc_transport_lwip_send;
	m_transport_client.recv = cc_transport_lwip_recv;
	m_transport_client.disconnect = cc_transport_lwip_disconnect;
	m_transport_client.free = cc_transport_lwip_free;

	return &m_transport_client;
}

cc_transport_client_t *cc_transport_lwip_get_client(void)
{
	return &m_transport_client;
}

bool cc_transport_lwip_has_data(cc_transport_client_t *transport_client)
{
	cc_transport_lwip_client_t *state = (cc_transport_lwip_client_t *)transport_client->client_state;

	return state->rx_buffer.size > 0;
}
