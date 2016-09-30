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
#include "app_error.h"
#include "lwip/inet6.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/netif.h"
#include "iot_defines.h"
#include "transport.h"
#include "platform.h"
#include "node.h"

result_t send_buffer(send_buffer_t *buf);

static transport_client_t *m_client;

// iface is the MAC address, convert it to a ipv6
// link-local address.
result_t discover_proxy(const char *iface, char *ip, int *port)
{
	long col1, col2, col3, col4, col5, col6;

	col1 = (unsigned char)strtol(iface, NULL, 16);
	col1 ^= 1 << 1;
	col2 = (unsigned char)strtol(iface + 3, NULL, 16);
	col3 = (unsigned char)strtol(iface + 6, NULL, 16);
	col4 = (unsigned char)strtol(iface + 9, NULL, 16);
	col5 = (unsigned char)strtol(iface + 12, NULL, 16);
	col6 = (unsigned char)strtol(iface + 15, NULL, 16);

	sprintf(ip,
		"fe80::%02lx%02lx:%02lxff:fe%02lx:%02lx%02lx",
		col1, col2, col3, col4, col5, col6);

	*port = 5000;

	return SUCCESS;
}

static err_t tcp_write_complete(void *p_arg, struct tcp_pcb *p_pcb, u16_t len)
{
	UNUSED_PARAMETER(p_arg);
	UNUSED_PARAMETER(p_pcb);

	log_debug("Write complete");

	m_client->tcp_state = TCP_STATE_CONNECTED;

	if (m_client->send_list != NULL) {
		send_buffer(m_client->send_list);
		m_client->send_list = m_client->send_list->next;
	}

	return ERR_OK;
}

result_t send_buffer(send_buffer_t *buf)
{
	result_t result = SUCCESS;
	err_t err = ERR_OK;
	struct tcp_pcb *pcb = m_client->tcp_port;
	uint32_t tcp_buffer_size = tcp_sndbuf(pcb);

	if (tcp_buffer_size >= buf->length) {
		err = tcp_write(pcb, buf->data, buf->length, 1);
		free(buf->data);
		free(buf);
		if (err != ERR_OK) {
			log_error("Failed to send TCP packet, reason %d", err);
			result = FAIL;
		} else {
			m_client->tcp_state = TCP_STATE_TCP_SEND_PENDING;
			tcp_sent(pcb, tcp_write_complete);
			log_debug("Sent: %d", buf->length);
		}
	} else {
		// TODO: Handle buf->length > tcp_buffer_size
		log_error("Failed to send TCP packet, buffer to big");
		result = FAIL;
	}

	return result;
}

result_t client_send(const transport_client_t *client, char *data, size_t len)
{
	result_t result = SUCCESS;
	send_buffer_t *new_buf = NULL, *tmp_buf = m_client->send_list;

	new_buf = (send_buffer_t *)malloc(sizeof(send_buffer_t));
	if (new_buf == NULL) {
		log_error("Failed to allocate memory");
		free(data);
		return FAIL;
	}

	new_buf->length = len + 4;
	new_buf->next = NULL;
	new_buf->data = data;
	new_buf->data[0] = (len & 0xFF000000);
	new_buf->data[1] = (len & 0x00FF0000);
	new_buf->data[2] = (len & 0x0000FF00) / 0x000000FF;
	new_buf->data[3] = (len & 0x000000FF);

	if (m_client->tcp_state == TCP_STATE_TCP_SEND_PENDING) {
		if (m_client->send_list == NULL) {
			m_client->send_list = new_buf;
		} else {
			while (tmp_buf->next != NULL)
				tmp_buf = tmp_buf->next;
			tmp_buf->next = new_buf;
		}
		return SUCCESS;
	}

	// TODO: Handle transmission failure
	result = send_buffer(new_buf);

	return result;
}

err_t tcp_recv_data_handler(void *p_arg, struct tcp_pcb *p_pcb, struct pbuf *p_buffer, err_t err)
{
	int read_pos = 0, msg_size = 0;

	if (err == ERR_OK && p_buffer != NULL) {
		tcp_recved(p_pcb, p_buffer->tot_len);

		while (read_pos < p_buffer->tot_len) {
			if (m_client->buffer == NULL) {
				msg_size = get_message_len(p_buffer->payload + read_pos);
				read_pos += 4; // Skip header
				if (msg_size <= p_buffer->tot_len - read_pos) {
					handle_data(p_buffer->payload + read_pos, msg_size);
					read_pos += msg_size;
				} else {
					m_client->buffer = malloc(msg_size);
					if (m_client->buffer == NULL) {
						log_error("Failed to allocate memory");
						return ERR_MEM;
					}
					memcpy(m_client->buffer, p_buffer->payload + read_pos, p_buffer->tot_len - read_pos);
					m_client->msg_size = msg_size;
					m_client->buffer_pos = p_buffer->tot_len - read_pos;
					return ERR_OK;
				}
			} else {
				msg_size = m_client->msg_size;
				if (msg_size - m_client->buffer_pos <= p_buffer->tot_len - read_pos) {
					memcpy(m_client->buffer + m_client->buffer_pos, p_buffer->payload + read_pos, msg_size - m_client->buffer_pos);
					handle_data(m_client->buffer, msg_size);
					free(m_client->buffer);
					m_client->buffer = NULL;
					read_pos += msg_size - m_client->buffer_pos;
					m_client->buffer = 0;
					m_client->msg_size = 0;
					m_client->buffer_pos = 0;
				} else {
					memcpy(m_client->buffer + m_client->buffer_pos, p_buffer->payload + read_pos, p_buffer->tot_len - read_pos);
					m_client->buffer_pos += p_buffer->tot_len - read_pos;
					return ERR_OK;
				}
			}
		}

		UNUSED_VARIABLE(pbuf_free(p_buffer));
	} else {
		log_debug("Error on receive, reason 0x%08x", err);
		if (p_buffer != NULL)
			UNUSED_VARIABLE(pbuf_free(p_buffer));
	}

	return ERR_OK;
}

static void tcp_error_handler(void *p_arg, err_t err)
{
	LWIP_UNUSED_ARG(p_arg);
	LWIP_UNUSED_ARG(err);

	log_debug("Error on TCP port, reason 0x%08x", err);
}

static err_t tcp_connection_poll(void *p_arg, struct tcp_pcb *p_pcb)
{
	LWIP_UNUSED_ARG(p_arg);
	LWIP_UNUSED_ARG(p_pcb);

	return ERR_OK;
}

static err_t tcp_connection_callback(void *p_arg, struct tcp_pcb *p_pcb, err_t err)
{
//    log_debug("TCP Connected, result 0x%08X.", err);

	APP_ERROR_CHECK(err);

	m_client->tcp_state = TCP_STATE_CONNECTED;

	tcp_setprio(p_pcb, TCP_PRIO_MIN);
	tcp_arg(p_pcb, NULL);
	tcp_recv(p_pcb, tcp_recv_data_handler);
	tcp_err(p_pcb, tcp_error_handler);
	tcp_poll(p_pcb, tcp_connection_poll, 0);

	client_connected();

	return ERR_OK;
}

transport_client_t *client_connect(const char *address, int port)
{
	ip6_addr_t ipv6_addr;
	err_t err;

	m_client = (transport_client_t *)malloc(sizeof(transport_client_t));
	if (m_client == NULL) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	if (ip6addr_aton(address, &ipv6_addr) != 1) {
		log_error("Failed to convert address");
		return NULL;
	}

	m_client->tcp_port = tcp_new_ip6();
	m_client->tcp_state = TCP_STATE_DISCONNECTED;
	m_client->state = TRANSPORT_DISCONNECTED;
	m_client->send_list = NULL;
	m_client->msg_size = 0;
	m_client->buffer = NULL;
	m_client->buffer_pos = 0;

	err = tcp_connect_ip6(m_client->tcp_port, &ipv6_addr, port, tcp_connection_callback);
	APP_ERROR_CHECK(err);
	log_debug("TCP connection requested to %s:%d.", address, port);

	return m_client;
}

result_t wait_for_data(transport_client_t **client, uint32_t timeout)
{
	return SUCCESS;
}
