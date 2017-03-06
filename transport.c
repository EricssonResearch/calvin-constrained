#ifndef __DMCE_PROBE_FUNCTION__HEADER__
#define __DMCE_PROBE_FUNCTION__HEADER__
static void dmce_probe_body(unsigned int probenbr);
#define DMCE_PROBE(a) (dmce_probe_body(a))
#endif
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
#include <string.h>
#include "transport.h"
#include "node.h"
#include "platform.h"
#include "proto.h"
#ifdef TRANSPORT_SOCKET
#include "transport_socket.h"
#endif
#ifdef TRANSPORT_LWIP
#include "transport_lwip.h"
#endif
#ifdef PLATFORM_ANDROID
#include "transport_fcm.h"
#endif

#define SERIALIZER "msgpack"

unsigned int get_message_len(const char *buffer)
{
	unsigned int value =
		((buffer[3] & 0xFF) <<  0) |
		((buffer[2] & 0xFF) <<  8) |
		((buffer[1] & 0xFF) << 16) |
		((buffer[0] & 0xFF) << 24);
	return value;
}

static result_t transport_handle_join_reply(node_t *node, transport_client_t *transport_client, char *data)
{
	char id[50] = "", serializer[20] = "", sid[50] = "";

	if (sscanf(data, "{\"cmd\": \"JOIN_REPLY\", \"id\": \"%[^\"]\", \"serializer\": \"%[^\"]\", \"sid\": \"%[^\"]\"}",
		id, serializer, sid) != 3) {
		log_error("Failed to parse JOIN_REPLY");
		transport_client->disconnect(transport_client);
		return FAIL;
	}

	if (strcmp(serializer, SERIALIZER) != 0) {
		log_error("Unsupported serializer");
		transport_client->disconnect(transport_client);
		return FAIL;
	}
	transport_client->state = TRANSPORT_ENABLED;
	strncpy(transport_client->peer_id, id, strlen(id)+1);

	log_debug("Transport joined '%s'", transport_client->peer_id);

	return SUCCESS;
}

void transport_handle_data(node_t *node, transport_client_t *transport_client, char *buffer, size_t len)
{
	unsigned int read_pos = 0, msg_size = 0;

	while (read_pos < len) {
		// New message?
		if (transport_client->rx_buffer.buffer == NULL) {
			msg_size = get_message_len(buffer + read_pos);
			read_pos += 4;

			// Complete message?
			if (msg_size <= len - read_pos) {
				if (transport_client->state == TRANSPORT_ENABLED)
					node_handle_message(node, buffer + read_pos, msg_size);
				else
					transport_handle_join_reply(node, transport_client, buffer + read_pos);
				read_pos += msg_size;
			} else {
				if (platform_mem_alloc((void **)&transport_client->rx_buffer.buffer, msg_size) != SUCCESS) {
					log_error("Failed to allocate rx buffer");
					return;
				}
				memcpy(transport_client->rx_buffer.buffer, buffer + read_pos, len - read_pos);
				transport_client->rx_buffer.pos = len - read_pos;
				transport_client->rx_buffer.size = msg_size;
				return;
			}
		} else {
			// Fragment completing message?
			if (transport_client->rx_buffer.size - transport_client->rx_buffer.pos <= len - read_pos) {
				memcpy(transport_client->rx_buffer.buffer + transport_client->rx_buffer.pos, buffer + read_pos, transport_client->rx_buffer.size - transport_client->rx_buffer.pos);
				if (transport_client->state == TRANSPORT_ENABLED)
					node_handle_message(node, transport_client->rx_buffer.buffer, transport_client->rx_buffer.size);
				else
					transport_handle_join_reply(node, transport_client, transport_client->rx_buffer.buffer);
				platform_mem_free((void *)transport_client->rx_buffer.buffer);
				read_pos += transport_client->rx_buffer.size - transport_client->rx_buffer.pos;
				transport_client->rx_buffer.buffer = NULL;
				transport_client->rx_buffer.pos = 0;
				transport_client->rx_buffer.size = 0;
			} else {
				memcpy(transport_client->rx_buffer.buffer + transport_client->rx_buffer.pos, buffer + read_pos, len - read_pos);
				transport_client->rx_buffer.pos += len - read_pos;
				return;
			}
		}
	}
}

result_t transport_create_tx_buffer(transport_client_t *transport_client, size_t size)
{
	if (transport_client->tx_buffer.buffer != NULL)
		return PENDING;

	if (platform_mem_alloc((void **)&transport_client->tx_buffer.buffer, size) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	transport_client->tx_buffer.pos = 0;
	transport_client->tx_buffer.size = 0;
	return SUCCESS;
}

result_t transport_create_rx_buffer(transport_client_t *transport_client, size_t size)
{
	if (transport_client->rx_buffer.buffer != NULL)
		return PENDING;

	if ((DMCE_PROBE(21),platform_mem_alloc((void **)&transport_client->rx_buffer.buffer, size) != SUCCESS)) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	(DMCE_PROBE(22),transport_client->rx_buffer.pos = 0);
	(DMCE_PROBE(23),transport_client->rx_buffer.size = 0);
	return SUCCESS;
}

void transport_free_tx_buffer(transport_client_t *transport_client)
{
	platform_mem_free((void *)transport_client->tx_buffer.buffer);
	transport_client->tx_buffer.pos = 0;
	transport_client->tx_buffer.size = 0;
	transport_client->tx_buffer.buffer = NULL;
}

void transport_free_rx_buffer(transport_client_t *transport_client)
{
	(DMCE_PROBE(24),platform_mem_free((void *)transport_client->rx_buffer.buffer));
	(DMCE_PROBE(25),transport_client->rx_buffer.pos = 0);
	(DMCE_PROBE(26),transport_client->rx_buffer.size = 0);
	transport_client->rx_buffer.buffer = NULL;
}

void transport_append_buffer_prefix(char *buffer, size_t size)
{
	buffer[0] = size >> 24 & 0xFF;
	buffer[1] = size >> 16 & 0xFF;
	buffer[2] = size >> 8 & 0xFF;
	buffer[3] = size & 0xFF;
}

void transport_join(node_t *node, transport_client_t *transport_client)
{
	if (proto_send_join_request(node, transport_client, SERIALIZER) == SUCCESS)
		transport_client->state = TRANSPORT_PENDING;
	else
		transport_client->disconnect(transport_client);
}

transport_client_t *transport_create(node_t *node, char *uri)
{
#ifdef TRANSPORT_SOCKET
	if (strncmp(uri, "calvinip://", 11) == 0 || strncmp(uri, "ssdp", 4) == 0)
		return transport_socket_create(node, uri);
#endif

#ifdef TRANSPORT_LWIP
	if (strncmp(uri, "lwip", 4) == 0)
		return transport_lwip_create(node);
#endif
#ifdef PLATFORM_ANDROID
	if (strncmp(uri, "calvinfcm://", 12) == 0)
		return transport_fcm_create(node, uri);
#endif

	log_error("No transport for '%s'", uri);

	return NULL;
}
/* This is a simple template for a linux userspace probe using the printf on stderr  */
#ifndef __DMCE_PROBE_FUNCTION_BODY__
#define __DMCE_PROBE_FUNCTION_BODY__

#include <stdio.h>

#define MAX_NUMBER_OF_PROBES 100000

static int dmce_probes[MAX_NUMBER_OF_PROBES] = {0};

static void dmce_probe_body(unsigned int probenbr)
{

  if (dmce_probes[probenbr] != 1)
  {
    dmce_probes[probenbr] = 1;
    fprintf(stderr, "\nDMCE_PROBE(%u)\n ",probenbr);
  }
}
#endif //__DMCE_PROBE_FUNCTION_BODY__
