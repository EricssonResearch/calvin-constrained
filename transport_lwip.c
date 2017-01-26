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

static transport_client_t m_client;
static volatile bool m_pending_transfer;

result_t transport_mac_to_link_local(const char *mac, char *ip)
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

	if (err == ERR_OK && p_buffer != NULL) {
		tcp_recved(p_pcb, p_buffer->tot_len);

		while (read_pos < p_buffer->tot_len) {
			// New message?
			if (m_client.rx_buffer.buffer == NULL) {
				msg_size = get_message_len(p_buffer->payload + read_pos);
				read_pos += 4;

				// Complete message?
				if (msg_size <= p_buffer->tot_len - read_pos) {
					node_handle_data(p_buffer->payload + read_pos, msg_size);
					read_pos += msg_size;
				} else {
					if (platform_mem_alloc((void **)&m_client.rx_buffer.buffer, msg_size) != SUCCESS) {
						log_error("Failed to allocate rx buffer");
						return ERR_MEM;
					}
					memcpy(m_client.rx_buffer.buffer, p_buffer->payload + read_pos, p_buffer->tot_len - read_pos);
					m_client.rx_buffer.pos = p_buffer->tot_len - read_pos;
					m_client.rx_buffer.size = msg_size;
					return ERR_OK;
				}
			} else {
				// Fragment completing message?
				if (m_client.rx_buffer.size - m_client.rx_buffer.pos <= p_buffer->tot_len - read_pos) {
					memcpy(m_client.rx_buffer.buffer + m_client.rx_buffer.pos, p_buffer->payload + read_pos, m_client.rx_buffer.size - m_client.rx_buffer.pos);
					node_handle_data(m_client.rx_buffer.buffer, m_client.rx_buffer.size);
					platform_mem_free((void *)m_client.rx_buffer.buffer);
					read_pos += m_client.rx_buffer.size - m_client.rx_buffer.pos;
					m_client.rx_buffer.buffer = NULL;
					m_client.rx_buffer.pos = 0;
					m_client.rx_buffer.size = 0;
				} else {
					memcpy(m_client.rx_buffer.buffer + m_client.rx_buffer.pos, p_buffer->payload + read_pos, p_buffer->tot_len - read_pos);
					m_client.rx_buffer.pos += p_buffer->tot_len - read_pos;
					return ERR_OK;
				}
			}
		}
		UNUSED_VARIABLE(pbuf_free(p_buffer));
	} else {
		log_error("Error on receive, reason 0x%08x", err);
		if (p_buffer != NULL)
			UNUSED_VARIABLE(pbuf_free(p_buffer));
	}

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

static void transport_free_tx_buffer(void)
{
	platform_mem_free((void *)m_client.tx_buffer.buffer);
	m_client.tx_buffer.pos = 0;
	m_client.tx_buffer.size = 0;
	m_client.tx_buffer.buffer = NULL;
	m_pending_transfer = false;
}

static err_t transport_write_complete(void *p_arg, struct tcp_pcb *p_pcb, u16_t len)
{
	UNUSED_PARAMETER(p_arg);
	err_t err = ERR_OK;
	uint32_t tcp_buffer_size = 0;

	// Complete message sent?
	if (m_client.tx_buffer.pos == m_client.tx_buffer.size) {
		transport_free_tx_buffer();
		node_transmit();
		return ERR_OK;
	}

	// Continue sending
	tcp_buffer_size = tcp_sndbuf(p_pcb);
	if (tcp_buffer_size >= m_client.tx_buffer.size - m_client.tx_buffer.pos) {
		err = tcp_write(p_pcb, m_client.tx_buffer.buffer + m_client.tx_buffer.pos, m_client.tx_buffer.size - m_client.tx_buffer.pos, 1);
		if (err == ERR_OK)
			m_client.tx_buffer.pos = m_client.tx_buffer.size;
		else
			log_error("TODO: Handle tx failures");
	} else if (tcp_buffer_size > 0) {
		err = tcp_write(p_pcb, m_client.tx_buffer.buffer + m_client.tx_buffer.pos, tcp_buffer_size, 1);
		if (err == ERR_OK)
			m_client.tx_buffer.pos += tcp_buffer_size;
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
	if (err != ERR_OK) {
		log_error("Failed to connect");
		return err;
	}

	log("TCP client connected");

	m_client.state = TRANSPORT_CONNECTED;

	tcp_setprio(p_pcb, TCP_PRIO_MIN);
	tcp_arg(p_pcb, NULL);
	tcp_recv(p_pcb, transport_recv_data_handler);
	tcp_err(p_pcb, transport_error_handler);
	tcp_poll(p_pcb, transport_connection_poll, 0);
	tcp_sent(p_pcb, transport_write_complete);

	node_transmit();

	return ERR_OK;
}

// TODO: transport_send should block/sleep until tx has finished
result_t transport_send(size_t len)
{
	err_t res = ERR_BUF;
	uint32_t tcp_buffer_size = 0;
	struct tcp_pcb *pcb = m_client.tcp_port;

	if (!m_pending_transfer) {
		m_client.tx_buffer.buffer[0] = len >> 24 & 0xFF;
		m_client.tx_buffer.buffer[1] = len >> 16 & 0xFF;
		m_client.tx_buffer.buffer[2] = len >> 8 & 0xFF;
		m_client.tx_buffer.buffer[3] = len & 0xFF;

		tcp_buffer_size = tcp_sndbuf(pcb);
		if (tcp_buffer_size >= len + 4) {
			tcp_sent(pcb, transport_write_complete);
			m_pending_transfer = true;
			res = tcp_write(pcb, m_client.tx_buffer.buffer, len + 4, 1);
			if (res == ERR_OK) {
				m_client.tx_buffer.pos = len + 4;
				m_client.tx_buffer.size = m_client.tx_buffer.pos;
			}
		} else if (tcp_buffer_size > 0) {
			tcp_sent(pcb, transport_write_complete);
			m_pending_transfer = true;
			res = tcp_write(pcb, m_client.tx_buffer.buffer, tcp_buffer_size, 1);
			if (res == ERR_OK) {
				m_client.tx_buffer.size = len + 4;
				m_client.tx_buffer.pos = tcp_buffer_size;
			}
		} else
			log_error("No space in send buffer");
	}

	if (res != ERR_OK) {
		transport_free_tx_buffer();
		return FAIL;
	}

	return SUCCESS;
}

result_t transport_start(const char *ssdp_iface, const char *proxy_iface, const int proxy_port)
{
	ip6_addr_t ipv6_addr;
	err_t err;
	char ip[40];
	int port = 0;

	if (transport_mac_to_link_local(proxy_iface, ip) != SUCCESS) {
		log_error("Failed to convert MAC address");
		return FAIL;
	}

	if (ip6addr_aton(ip, &ipv6_addr) != 1) {
		log_error("Failed to convert IP address");
		return FAIL;
	}

	m_client.tcp_port = tcp_new_ip6();
	m_client.state = TRANSPORT_DISCONNECTED;
	m_client.rx_buffer.buffer = NULL;
	m_client.rx_buffer.pos = 0;
	m_client.rx_buffer.size = 0;
	m_client.tx_buffer.buffer = NULL;
	m_client.tx_buffer.pos = 0;
	m_client.tx_buffer.size = 0;
	m_pending_transfer = false;

	err = tcp_connect_ip6(m_client.tcp_port, &ipv6_addr, proxy_port, transport_connection_callback);
	if (err != ERR_OK) {
		log_error("Failed to connect socket");
		return FAIL;
	}

	log("TCP connection requested to %s:%d.", ip, proxy_port);

	return SUCCESS;
}

void transport_stop(void)
{
	m_client.state = TRANSPORT_DISCONNECTED;
}

void transport_set_state(const transport_state_t state)
{
	m_client.state = state;
}

transport_state_t transport_get_state(void)
{
	return m_client.state;
}

result_t transport_select(uint32_t timeout)
{
	return SUCCESS;
}

result_t transport_get_tx_buffer(char **buffer, uint32_t size)
{
	if (m_pending_transfer)
		return FAIL;

	if (platform_mem_alloc((void **)buffer, size) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	m_client.tx_buffer.pos = 0;
	m_client.tx_buffer.size = 0;
	m_client.tx_buffer.buffer = *buffer;
	return SUCCESS;
}

bool transport_can_transmit(void)
{
	return !m_pending_transfer;
}
