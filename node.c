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
#include <string.h>
#include "node.h"
#include "platform.h"
#include "proto.h"

#define SERIALIZER "msgpack"
#define SCHEMA "calvinip"
#define STORAGE_TUNNEL "storage"
#define JOIN_REQUEST_BUF_SIZE 160

static node_t *m_node;

result_t add_pending_msg_with_data(msg_type_t type, char *msg_uuid, void *data)
{
	pending_msg_t *new_msg = NULL, *tmp_msg = NULL;

	new_msg = (pending_msg_t*)malloc(sizeof(pending_msg_t));
	if (new_msg == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	new_msg->msg_uuid = msg_uuid;
	new_msg->type = type;
	new_msg->data = data;
	new_msg->next = NULL;

	if (m_node->pending_msgs == NULL) {
		m_node->pending_msgs = new_msg;
	} else {
		tmp_msg = m_node->pending_msgs;
		while (tmp_msg->next != NULL) {
			tmp_msg = tmp_msg->next;
		}
		tmp_msg->next = new_msg;
	}

	return SUCCESS;	
}

result_t add_pending_msg(msg_type_t type, char *msg_uuid)
{
	return add_pending_msg_with_data(type, msg_uuid, NULL);
}

result_t remove_pending_msg(char *msg_uuid)
{
	pending_msg_t *tmp_msg = m_node->pending_msgs, *prev_msg = m_node->pending_msgs;

	if (tmp_msg != NULL && strcmp(tmp_msg->msg_uuid, msg_uuid) == 0) {
		m_node->pending_msgs = tmp_msg->next;
		free(tmp_msg->msg_uuid);
		free(tmp_msg);
		return SUCCESS;
	}

	while (tmp_msg != NULL && strcmp(tmp_msg->msg_uuid, msg_uuid) != 0) {
		prev_msg = tmp_msg;
		tmp_msg = tmp_msg->next;
	}

	if (tmp_msg == NULL) {
		return FAIL;
	}

	prev_msg->next = tmp_msg->next;

	free(tmp_msg->msg_uuid);
	if (tmp_msg->data) {
		free(tmp_msg->data);
	}
	free(tmp_msg);

	return SUCCESS;
}

result_t send_join_request(transport_client_t *client, char *node_id, const char *serializer)
{
	char *message, *msg_id;
	int size = 0;

	message = malloc(JOIN_REQUEST_BUF_SIZE);
	if (message == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	msg_id = gen_uuid("MSGID_");
	size = sprintf(message + 4,
		"{\"cmd\": \"JOIN_REQUEST\", \"id\": \"%s\", \"sid\": \"%s\", \"serializers\": [\"%s\"]}",
		node_id,
		msg_id,
		serializer);
	free(msg_id);

	return client_send(client, message, size);
}

result_t handle_join_reply(char *data)
{
	result_t result = SUCCESS;
	char *msg_uuid_tunnel = NULL, *msg_uuid_node_setup = NULL, id[50] = "", serializer[20] = "", sid[50] = "";

	sscanf(data, "{\"cmd\": \"JOIN_REPLY\", \"id\": \"%[^\"]\", \"serializer\": \"%[^\"]\", \"sid\": \"%[^\"]\"}",
		id, serializer, sid);

	if (strcmp(serializer, SERIALIZER) != 0) {
		log_error("Unsupported serializer");
		return FAIL;
	}

	m_node->client->state = TRANSPORT_JOINED;
	m_node->storage_tunnel->peer_id = strdup(id);
	m_node->storage_tunnel->ref_count++;
	m_node->storage_tunnel->tunnel_id = gen_uuid("TUNNEL_");

	msg_uuid_tunnel = gen_uuid("MSGID_");
	if (send_tunnel_request(msg_uuid_tunnel, m_node->storage_tunnel, m_node->node_id, STORAGE_TUNNEL) != SUCCESS) {
		log_error("Failed to create storage tunnel");
		free(msg_uuid_tunnel);
		result = FAIL;
	} else {
		result = add_pending_msg(TUNNEL_NEW, msg_uuid_tunnel);
	}

	if (result == SUCCESS) {
		msg_uuid_node_setup = gen_uuid("MSGID_");
		result = send_node_setup(msg_uuid_node_setup, m_node->node_id, id, m_node->vid, m_node->pid, m_node->client);
		if (result == SUCCESS) {
			result = add_pending_msg(PROXY_CONFIG, msg_uuid_node_setup);
		} else {
			log_error("Failed send node setup request");
			free(msg_uuid_node_setup);
		}
	}

	return result;
}

result_t handle_token(char *port_id, token_t *token, uint32_t sequencenbr)
{
	port_t *port = get_inport(port_id);

	if (port != NULL) {
		if (fifo_write(port->fifo, token) == SUCCESS) {
			return send_token_reply(
				m_node->node_id,
				port,
				sequencenbr,
				"ACK");
		} else {
			send_token_reply(
				m_node->node_id,
				port,
				sequencenbr,
				"NACK");
			free_token(token);
		}
	} else {
		log_error("No port with id '%s'", port_id);
	}

	return FAIL;
}

void handle_data(char *data, int len, transport_client_t *connection)
{
	switch (m_node->client->state) {
		case TRANSPORT_CONNECTED:
			handle_join_reply(data);
			break;
		case TRANSPORT_JOINED:
			parse_message(m_node, data, connection);
			break;
		default:
			log_error("Received data in unhandled state");
	}
}

void handle_token_reply(char *port_id, bool acked)
{
	port_t *port = get_outport(port_id);

	if (port != NULL) {
		fifo_commit_read(port->fifo, acked, true);
	} else {
		log_error("No port with id '%s'", port_id);
	}
}

result_t move_token(port_t *port, token_t *token)
{
	port_t *peer_port = NULL;

	if (port->local_connection == NULL) {
		peer_port = get_inport(port->peer_port_id);
		if (peer_port == NULL) {
			log_error("No port with id '%s'", port->peer_port_id);
			return FAIL;
		} else {
			port->local_connection = peer_port;
		}
	}

	if (fifo_can_write(port->local_connection->fifo)) {
		return fifo_write(port->local_connection->fifo, token);
	}

	return FAIL;
}

result_t send_tokens(actor_t *actor)
{
	result_t result = SUCCESS;
	port_t *port = NULL;
	token_t *token = NULL;

	port = actor->outports;
	while (port != NULL && result == SUCCESS) {
		if (port->fifo != NULL) {
			while (fifo_can_read(port->fifo) && result == SUCCESS) {
				token = fifo_read(port->fifo);
				if (token != NULL) {
					if (port->is_local) {
						result = move_token(port, token);
						if (result == SUCCESS) {
							fifo_commit_read(port->fifo, true, false);
						}
					} else {
						// commited when ACK received
						result = send_token(m_node->node_id, port, token);
					}

					if (result == FAIL) {
						fifo_commit_read(port->fifo, false, false);
					}
				} else {
					result = FAIL;
				}
			}
		}
		port = port->next;
	}

	return result;
}

void client_connected()
{
	log_debug("Client connected");

	m_node->client->state = TRANSPORT_CONNECTED;

	if (send_join_request(m_node->client, m_node->node_id, SERIALIZER) != SUCCESS) {
		log_error("Failed to send join request");
	}
}

node_t *get_node()
{
	return m_node;
}

result_t create_node(uint32_t vid, uint32_t pid, char *name)
{
	int i = 0;

	m_node = (node_t*)malloc(sizeof(node_t));
	if (m_node == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	for (i = 0; i < MAX_ACTORS; i++) {
		m_node->actors[i] = NULL;
	}

	for (i = 0; i < MAX_TUNNELS; i++) {
		m_node->tunnels[i] = NULL;
	}

	m_node->vid = vid;
	m_node->pid = pid;
	m_node->node_id = gen_uuid(NULL);
	m_node->schema = SCHEMA;
	m_node->pending_msgs = NULL;
	m_node->name = name;
	m_node->client = NULL;
	m_node->storage_tunnel = NULL;

	return SUCCESS;
}

result_t start_node(char *interface)
{
	if (discover_proxy(interface, m_node->proxy_ip, &m_node->proxy_port) != SUCCESS) {
		log_error("No proxy found");
		return FAIL;
	}

	log_debug("Found proxy %s:%d", m_node->proxy_ip, m_node->proxy_port);

	m_node->client = client_connect(m_node->proxy_ip, m_node->proxy_port);
	if (m_node->client == NULL) {
		log_error("Failed to connect to proxy");
		return FAIL;
	}

	m_node->storage_tunnel = (tunnel_t*)malloc(sizeof(tunnel_t));
	if (m_node->storage_tunnel == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	m_node->storage_tunnel->connection = m_node->client;

	if (m_node->client->state == TRANSPORT_CONNECTED) {
		client_connected();
	}

	return SUCCESS;
}

result_t loop_once()
{
	result_t result = SUCCESS;
	int i_actor = 0;
	actor_t *actor = NULL;

	for (i_actor = 0; i_actor < MAX_ACTORS && result == SUCCESS; i_actor++) {
		actor = m_node->actors[i_actor];
		if (actor != NULL && actor->enabled) {
			result = actor->fire(actor);
			if (result == SUCCESS) {
				result = send_tokens(actor);
			}
		}
	}

	return result;
}

void node_run()
{
	// TODO: Set timeout based on active timers
	uint32_t timeout = 1;
	while (1) {
		if (wait_for_data(&m_node->client, timeout) == SUCCESS) {
			loop_once();
		}

		if (m_node->client == NULL || (m_node->client != NULL
			&& m_node->client->state == TRANSPORT_DISCONNECTED)) {
			stop_node(false);
//			start_node();
		}
	}
}

void stop_node(bool terminate)
{
	pending_msg_t *tmp_msg = NULL;
	int i_actor = 0, i_tunnel = 0;

	log_debug("Stopping node");

	if (m_node->client != NULL) {
		free_client(m_node->client);
		m_node->client = NULL;
	}

	if (m_node->storage_tunnel != NULL) {
		free_tunnel(m_node->storage_tunnel);
		m_node->storage_tunnel = NULL;
	}

	if (m_node->pending_msgs != NULL) {
		tmp_msg = m_node->pending_msgs;
		while (tmp_msg != NULL) {
			m_node->pending_msgs = m_node->pending_msgs->next;
			free(tmp_msg);
			tmp_msg = m_node->pending_msgs;
		}
		m_node->pending_msgs = NULL;
	}

	// TODO: If not terminating, try to reconnect actors
	// when new proxy connection is made.
	for (i_actor = 0; i_actor < MAX_ACTORS; i_actor++) {
		free_actor(m_node->actors[i_actor], false);
		m_node->actors[i_actor] = NULL;
	}

	for (i_tunnel = 0; i_tunnel < MAX_TUNNELS; i_tunnel++) {
		free_tunnel(m_node->tunnels[i_tunnel]);
		m_node->tunnels[i_tunnel] = NULL;
	}

	if (terminate) {
		if (m_node->node_id != NULL) {
			free(m_node->node_id);
		}

		if (m_node->name != NULL) {
			free(m_node->name);
		}

		free(m_node);

		exit(EXIT_SUCCESS);
	}
}