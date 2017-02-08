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
#include "platform.h"
#include "node.h"

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
	node_t *node = node_get();
	int read_pos = 0, msg_size = 0;

	if (err == ERR_OK) {
		tcp_recved(p_pcb, p_buffer->tot_len);
		transport_handle_data(node->transport_client, p_buffer->payload, p_buffer->tot_len);
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
	node_t *node = node_get();

	// Complete message sent?
	if (node->transport_client->tx_buffer.pos == node->transport_client->tx_buffer.size) {
		node->transport_client->has_pending_tx = false;
		transport_free_tx_buffer(node->transport_client);
		return ERR_OK;
	}

	// Continue sending
	tcp_buffer_size = tcp_sndbuf(p_pcb);
	if (tcp_buffer_size >= node->transport_client->tx_buffer.size - node->transport_client->tx_buffer.pos) {
		err = tcp_write(p_pcb, node->transport_client->tx_buffer.buffer + node->transport_client->tx_buffer.pos, node->transport_client->tx_buffer.size - node->transport_client->tx_buffer.pos, 1);
		if (err == ERR_OK)
			node->transport_client->tx_buffer.pos = node->transport_client->tx_buffer.size;
		else
			log_error("TODO: Handle tx failures");
	} else if (tcp_buffer_size > 0) {
		err = tcp_write(p_pcb, node->transport_client->tx_buffer.buffer + node->transport_client->tx_buffer.pos, tcp_buffer_size, 1);
		if (err == ERR_OK)
			node->transport_client->tx_buffer.pos += tcp_buffer_size;
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
	node_t *node = node_get();

	if (err != ERR_OK) {
		log_error("Failed to create TCP connection");
		return err;
	}

	log("TCP client connected");

	node->transport_client->state = TRANSPORT_DO_JOIN;

	tcp_setprio(p_pcb, TCP_PRIO_MIN);
	tcp_arg(p_pcb, NULL);
	tcp_recv(p_pcb, transport_recv_data_handler);
	tcp_err(p_pcb, transport_error_handler);
	tcp_poll(p_pcb, transport_connection_poll, 0);
	tcp_sent(p_pcb, transport_write_complete);

	return ERR_OK;
}

// TODO: transport_send should block/sleep until tx has finished
result_t transport_send(transport_client_t *transport_client, size_t size)
{
	err_t res = ERR_BUF;
	uint32_t tcp_buffer_size = 0;
	node_t *node = node_get();
	struct tcp_pcb *pcb = node->transport_client->tcp_port;

	if (!node->transport_client->has_pending_tx) {
		transport_append_buffer_prefix(node->transport_client->tx_buffer.buffer, size);

		tcp_buffer_size = tcp_sndbuf(pcb);
		if (tcp_buffer_size >= size + 4) {
			tcp_sent(pcb, transport_write_complete);
			res = tcp_write(pcb, node->transport_client->tx_buffer.buffer, size + 4, 1);
			if (res == ERR_OK) {
				node->transport_client->has_pending_tx = true;
				node->transport_client->tx_buffer.pos = size + 4;
				node->transport_client->tx_buffer.size = node->transport_client->tx_buffer.pos;
			}
		} else if (tcp_buffer_size > 0) {
			tcp_sent(pcb, transport_write_complete);
			res = tcp_write(pcb, node->transport_client->tx_buffer.buffer, tcp_buffer_size, 1);
			if (res == ERR_OK) {
				node->transport_client->has_pending_tx = true;
				node->transport_client->tx_buffer.size = size + 4;
				node->transport_client->tx_buffer.pos = tcp_buffer_size;
			}
		} else
			log_error("No space in send buffer");
	}

	if (res != ERR_OK) {
		transport_free_tx_buffer(node->transport_client);
		return FAIL;
	}

	return SUCCESS;
}

transport_client_t *transport_create(char *uri)
{
	transport_client_t *transport_client = NULL;

	if (platform_mem_alloc((void **)&transport_client, sizeof(transport_client_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	transport_client->tcp_port = tcp_new_ip6();
	transport_client->state = TRANSPORT_INTERFACE_DOWN;
	transport_client->rx_buffer.buffer = NULL;
	transport_client->rx_buffer.pos = 0;
	transport_client->rx_buffer.size = 0;
	transport_client->tx_buffer.buffer = NULL;
	transport_client->tx_buffer.pos = 0;
	transport_client->tx_buffer.size = 0;
	transport_client->has_pending_tx = false;

	return transport_client;
}

result_t transport_connect(transport_client_t *transport_client)
{
	ip6_addr_t ipv6_addr;
	err_t err;
	char ip[40];

	if (transport_convert_mac_to_link_local(transport_client->uri, ip) != SUCCESS) {
		log_error("Failed to convert MAC address '%s'", transport_client->uri);
		return FAIL;
	}

	if (!ip6addr_aton(ip, &ipv6_addr)) {
		log_error("Failed to convert IP address");
		return FAIL;
	}

	err = tcp_connect_ip6(transport_client->tcp_port, &ipv6_addr, 5000, transport_connection_callback);
	if (err != ERR_OK)
		return FAIL;

	transport_client->state = TRANSPORT_PENDING;

	log("TCP connection requested to %s:%d.", ip, 5000);

	return SUCCESS;
}

void transport_disconnect(transport_client_t *transport_client)
{
	tcp_close(transport_client->tcp_port);
	transport_client->state = TRANSPORT_DISCONNECTED;
}
