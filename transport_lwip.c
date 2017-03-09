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
#include "transport.h"
#include "transport_lwip.h"
#include "platform.h"
#include "node.h"

static transport_client_t m_transport_client;

result_t transport_convert_mac_to_link_local(const char *mac, char *ip)
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

	return SUCCESS;
}

err_t transport_recv_data_handler(void *p_arg, struct tcp_pcb *p_pcb, struct pbuf *p_buffer, err_t err)
{
	int read_pos = 0, msg_size = 0;

	if (err == ERR_OK) {
		tcp_recved(p_pcb, p_buffer->tot_len);
		transport_handle_data(((transport_lwip_client_t *)m_transport_client.client_state)->node, &m_transport_client, p_buffer->payload, p_buffer->tot_len);
	} else
		log_error("Error on receive, reason 0x%08x", err);

	if (p_buffer != NULL)
		UNUSED_VARIABLE(pbuf_free(p_buffer));

	return ERR_OK;
}

static void transport_error_handler(void *p_arg, err_t err)
{
	LWIP_UNUSED_ARG(p_arg);
	LWIP_UNUSED_ARG(err);

	log_error("Error on TCP port, reason 0x%08x", err);
}

static err_t transport_connection_poll(void *p_arg, struct tcp_pcb *p_pcb)
{
	LWIP_UNUSED_ARG(p_arg);
	LWIP_UNUSED_ARG(p_pcb);

	return ERR_OK;
}

static err_t transport_write_complete(void *p_arg, struct tcp_pcb *p_pcb, u16_t len)
{
	UNUSED_PARAMETER(p_arg);
	err_t err = ERR_OK;
	uint32_t tcp_buffer_size = 0;
	transport_lwip_client_t *transport_lwip = (transport_lwip_client_t *)m_transport_client.client_state;

	// Complete message sent?
	if (m_transport_client.tx_buffer.pos == m_transport_client.tx_buffer.size) {
		transport_lwip->has_pending_tx = false;
		transport_free_tx_buffer(&m_transport_client);
		return ERR_OK;
	}

	// Continue sending
	tcp_buffer_size = tcp_sndbuf(p_pcb);
	if (tcp_buffer_size >= m_transport_client.tx_buffer.size - m_transport_client.tx_buffer.pos) {
		err = tcp_write(p_pcb, m_transport_client.tx_buffer.buffer + m_transport_client.tx_buffer.pos, m_transport_client.tx_buffer.size - m_transport_client.tx_buffer.pos, 1);
		if (err == ERR_OK)
			m_transport_client.tx_buffer.pos = m_transport_client.tx_buffer.size;
		else
			log_error("TODO: Handle tx failures");
	} else if (tcp_buffer_size > 0) {
		err = tcp_write(p_pcb, m_transport_client.tx_buffer.buffer + m_transport_client.tx_buffer.pos, tcp_buffer_size, 1);
		if (err == ERR_OK)
			m_transport_client.tx_buffer.pos += tcp_buffer_size;
		else
			log_error("TODO: Handle tx failures");
	} else {
		log_error("No space in send buffer");
		err = ERR_MEM;
	}

	return err;
}

static err_t transport_connection_callback(void *p_arg, struct tcp_pcb *p_pcb, err_t err)
{
	transport_lwip_client_t *transport_state = (transport_lwip_client_t *)m_transport_client.client_state;

	if (err != ERR_OK) {
		log_error("Failed to create TCP connection");
		return err;
	}

	log("TCP client connected");

	tcp_setprio(p_pcb, TCP_PRIO_MIN);
	tcp_arg(p_pcb, NULL);
	tcp_recv(p_pcb, transport_recv_data_handler);
	tcp_err(p_pcb, transport_error_handler);
	tcp_poll(p_pcb, transport_connection_poll, 0);
	tcp_sent(p_pcb, transport_write_complete);

	transport_join(transport_state->node, &m_transport_client);

	return ERR_OK;
}

// TODO: transport_send should block/sleep until tx has finished
static result_t transport_lwip_send_tx_buffer(node_t *node, transport_client_t *transport_client, size_t size)
{
	err_t res = ERR_BUF;
	uint32_t tcp_buffer_size = 0;
	transport_lwip_client_t *transport_lwip = (transport_lwip_client_t *)transport_client->client_state;
	struct tcp_pcb *pcb = transport_lwip->tcp_port;

	if (!transport_lwip->has_pending_tx) {
		transport_append_buffer_prefix(transport_client->tx_buffer.buffer, size);

		tcp_buffer_size = tcp_sndbuf(pcb);
		if (tcp_buffer_size >= size + 4) {
			tcp_sent(pcb, transport_write_complete);
			res = tcp_write(pcb, transport_client->tx_buffer.buffer, size + 4, 1);
			if (res == ERR_OK) {
				transport_lwip->has_pending_tx = true;
				transport_client->tx_buffer.pos = size + 4;
				transport_client->tx_buffer.size = transport_client->tx_buffer.pos;
			}
		} else if (tcp_buffer_size > 0) {
			tcp_sent(pcb, transport_write_complete);
			res = tcp_write(pcb, transport_client->tx_buffer.buffer, tcp_buffer_size, 1);
			if (res == ERR_OK) {
				transport_lwip->has_pending_tx = true;
				transport_client->tx_buffer.size = size + 4;
				transport_client->tx_buffer.pos = tcp_buffer_size;
			}
		} else
			log_error("No space in send buffer");
	}

	if (res != ERR_OK) {
		transport_free_tx_buffer(transport_client);
		return FAIL;
	}

	return SUCCESS;
}

static result_t transport_lwip_connect(node_t *node, transport_client_t *transport_client)
{
	transport_lwip_client_t *transport_lwip = (transport_lwip_client_t *)transport_client->client_state;
	ip6_addr_t ipv6_addr;
	err_t err;
	char ip[40];

	if (transport_convert_mac_to_link_local(transport_lwip->mac, ip) != SUCCESS) {
		log_error("Failed to convert MAC address '%s'", transport_lwip->mac);
		return FAIL;
	}

	if (!ip6addr_aton(ip, &ipv6_addr)) {
		log_error("Failed to convert IP address");
		return FAIL;
	}

	err = tcp_connect_ip6(transport_lwip->tcp_port, &ipv6_addr, 5000, transport_connection_callback);
	if (err != ERR_OK) {
		log_error("Failed to connect socket");
		return FAIL;
	}

	transport_client->state = TRANSPORT_PENDING;

	log("TCP connection requested to %s:%d.", ip, 5000);

	return SUCCESS;
}

static void transport_lwip_disconnect(node_t *node, transport_client_t *transport_client)
{
	transport_lwip_client_t *transport_lwip = (transport_lwip_client_t *)transport_client->client_state;

	tcp_close(transport_lwip->tcp_port);
	transport_client->state = TRANSPORT_DISCONNECTED;
}

static void transport_lwip_free(transport_client_t *transport_client)
{
	if (transport_client->client_state != NULL)
		platform_mem_free((void *)transport_client->client_state);
}

transport_client_t *transport_lwip_create(node_t *node, char *uri)
{
	transport_lwip_client_t *transport_lwip = NULL;

	if (platform_mem_alloc(&transport_lwip, sizeof(transport_lwip_client_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	memset(transport_lwip, 0, sizeof(transport_lwip_client_t));
	transport_lwip->node = node;
	transport_lwip->tcp_port = tcp_new_ip6();
	transport_lwip->has_pending_tx = false;
	m_transport_client.client_state = transport_lwip;
	m_transport_client.state = TRANSPORT_INTERFACE_DOWN;
	m_transport_client.rx_buffer.buffer = NULL;
	m_transport_client.rx_buffer.pos = 0;
	m_transport_client.rx_buffer.size = 0;
	m_transport_client.tx_buffer.buffer = NULL;
	m_transport_client.tx_buffer.pos = 0;
	m_transport_client.tx_buffer.size = 0;
	m_transport_client.connect = transport_lwip_connect;
	m_transport_client.send_tx_buffer = transport_lwip_send_tx_buffer;
	m_transport_client.disconnect = transport_lwip_disconnect;
	m_transport_client.free = transport_lwip_free;

	return &m_transport_client;
}

transport_client_t *transport_get_client(void)
{
	return &m_transport_client;
}
