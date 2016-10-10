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
#include "msgpack_helper.h"

#define SERIALIZER "msgpack"
#define SCHEMA "calvinip"
#define JOIN_REQUEST_BUF_SIZE 160

static node_t *m_node;

result_t add_pending_msg(char *msg_uuid, result_t (*handler)(char *data, void *msg_data), void *msg_data)
{
	pending_msg_t *new_msg = NULL, *tmp_msg = NULL;

	new_msg = (pending_msg_t *)malloc(sizeof(pending_msg_t));
	if (new_msg == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	new_msg->msg_uuid = msg_uuid;
	new_msg->handler = handler;
	new_msg->msg_data = msg_data;
	new_msg->next = NULL;

	if (m_node->pending_msgs == NULL) {
		m_node->pending_msgs = new_msg;
	} else {
		tmp_msg = m_node->pending_msgs;
		while (tmp_msg->next != NULL)
			tmp_msg = tmp_msg->next;
		tmp_msg->next = new_msg;
	}

	return SUCCESS;
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

	if (tmp_msg == NULL)
		return FAIL;

	prev_msg->next = tmp_msg->next;

	free(tmp_msg->msg_uuid);
	free(tmp_msg);

	return SUCCESS;
}

static result_t send_join_request(void)
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
		m_node->node_id,
		msg_id,
		SERIALIZER);
	free(msg_id);

	return client_send(m_node->transport, message, size);
}

static result_t node_setup_reply_handler(char *data, void *msg_data)
{
	uint32_t status;
	char *value = NULL;

	if (get_value_from_map(&data, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(&value, "status", &status) == SUCCESS) {
			if (status == 200) {
				log("Node started and configured");
				return SUCCESS;
			}
		}
	}

	log_debug("Failed to configure node");

	return FAIL;
}

static result_t storage_tunnel_reply_handler(char *data, void *msg_data)
{
	uint32_t status;
	char *value = NULL, *tunnel_id = NULL, *data_value = NULL;
	result_t result = FAIL;

	result = get_value_from_map(&data, "value", &value);

	if (result == SUCCESS)
		result = get_value_from_map(&value, "data", &data_value);

	if (result == SUCCESS)
		result = decode_string_from_map(&data_value, "tunnel_id", &tunnel_id);

	if (result == SUCCESS)
		result = decode_uint_from_map(&value, "status", &status);

	if (result != SUCCESS)
		log_error("Failed to parse storage reply");

	if (result == SUCCESS && status == 200) {
		m_node->storage_tunnel->state = TUNNEL_WORKING;
		result = send_node_setup(m_node, node_setup_reply_handler);
		if (result != SUCCESS)
			log_error("Failed send node setup request");
	}

	if (tunnel_id != NULL)
		free(tunnel_id);

	return result;
}

static result_t handle_join_reply(char *data)
{
	char id[50] = "", serializer[20] = "", sid[50] = "";

	if (sscanf(data, "{\"cmd\": \"JOIN_REPLY\", \"id\": \"%[^\"]\", \"serializer\": \"%[^\"]\", \"sid\": \"%[^\"]\"}",
		id, serializer, sid) != 3) {
		log_error("Failed to parse JOIN_REPLY");
		return FAIL;
	}

	if (strcmp(serializer, SERIALIZER) != 0) {
		log_error("Unsupported serializer");
		return FAIL;
	}

	m_node->transport->state = TRANSPORT_JOINED;

	m_node->proxy_link = create_link(id, LINK_WORKING);
	if (m_node->proxy_link == NULL) {
		log_error("Failed to create link for proxy '%s'", id);
		return FAIL;
	}

	log_debug("Link created to proxy '%s'", id);

	m_node->storage_tunnel = create_tunnel(m_node->proxy_link, TUNNEL_TYPE_STORAGE, TUNNEL_PENDING);
	if (m_node->storage_tunnel == NULL) {
		log_error("Failed to create storage tunnel");
		free_link(m_node->proxy_link);
		m_node->proxy_link = NULL;
		return FAIL;
	}

	if (send_tunnel_request(m_node, m_node->storage_tunnel, storage_tunnel_reply_handler) != SUCCESS) {
		log_error("Failed to request storage tunnel");
		free_tunnel(m_node, m_node->storage_tunnel);
		m_node->storage_tunnel = NULL;
		return FAIL;
	}

	return SUCCESS;
}

result_t handle_token(char *port_id, token_t *token, uint32_t sequencenbr)
{
	port_t *port = get_inport(m_node, port_id);

	if (port != NULL && port->state == PORT_CONNECTED) {
		if (fifo_write(port->fifo, token) == SUCCESS) {
			return send_token_reply(m_node,
				port,
				sequencenbr,
				"ACK");
		} else {
			send_token_reply(m_node,
				port,
				sequencenbr,
				"NACK");
			free_token(token);
		}
	} else
		log_error("No port with id '%s'", port_id);

	return FAIL;
}

void handle_data(char *data, int len)
{
	switch (m_node->transport->state) {
	case TRANSPORT_CONNECTED:
		handle_join_reply(data);
		break;
	case TRANSPORT_JOINED:
		parse_message(m_node, data);
		break;
	default:
		log_error("Received data in unhandled state");
	}
}

void handle_token_reply(char *port_id, port_reply_type_t reply_type)
{
	port_t *port = get_outport(m_node, port_id);

	if (port != NULL) {
		if (reply_type == PORT_REPLY_TYPE_ACK)
			fifo_commit_read(port->fifo, true, true);
		else if (reply_type == PORT_REPLY_TYPE_ABORT)
			log_error("TODO: handle ABORT");
	} else
		log_error("No port with id '%s'", port_id);
}

static result_t move_token(port_t *port, token_t *token)
{
	port_t *peer_port = NULL;

	if (port->local_connection == NULL) {
		peer_port = get_inport(m_node, port->peer_port_id);
		if (peer_port == NULL) {
			log_error("No port with id '%s'", port->peer_port_id);
			return FAIL;
		}
		port->local_connection = peer_port;
	}

	if (fifo_can_write(port->local_connection->fifo))
		return fifo_write(port->local_connection->fifo, token);

	return FAIL;
}

static result_t send_tokens(actor_t *actor)
{
	result_t result = SUCCESS;
	port_t *port = NULL;
	token_t *token = NULL;

	port = actor->outports;
	while (port != NULL && port->fifo != NULL && result == SUCCESS) {
		while (fifo_can_read(port->fifo) && result == SUCCESS) {
			token = fifo_read(port->fifo);
			if (token != NULL) {
				if (port->is_local) {
					result = move_token(port, token);
					if (result == SUCCESS)
						fifo_commit_read(port->fifo, true, false);
				} else
					result = send_token(m_node, port, token);

				if (result == FAIL)
					fifo_commit_read(port->fifo, false, false);
			}
		}
		port = port->next;
	}

	return result;
}

void client_connected(void)
{
	log_debug("Client connected");

	m_node->transport->state = TRANSPORT_CONNECTED;

	if (send_join_request() != SUCCESS)
		log_error("Failed to send join request");
}

node_t *get_node()
{
	return m_node;
}

result_t create_node(uint32_t vid, uint32_t pid, char *name)
{
	int i = 0;

	m_node = (node_t *)malloc(sizeof(node_t));
	if (m_node == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	for (i = 0; i < MAX_ACTORS; i++)
		m_node->actors[i] = NULL;

	for (i = 0; i < MAX_TUNNELS; i++)
		m_node->tunnels[i] = NULL;

	for (i = 0; i < MAX_LINKS; i++)
		m_node->links[i] = NULL;

	m_node->vid = vid;
	m_node->pid = pid;
	m_node->node_id = gen_uuid(NULL);
	m_node->schema = SCHEMA;
	m_node->pending_msgs = NULL;
	m_node->name = name;
	m_node->transport = NULL;
	m_node->storage_tunnel = NULL;
	m_node->proxy_link = NULL;

	log_debug("Created node with id '%s'", m_node->node_id);

	return SUCCESS;
}

result_t start_node(const char *interface)
{
	if (discover_proxy(interface, m_node->proxy_ip, &m_node->proxy_port) != SUCCESS) {
		log_error("No proxy found");
		return FAIL;
	}

	log_debug("Found proxy %s:%d", m_node->proxy_ip, m_node->proxy_port);

	m_node->transport = client_connect(m_node->proxy_ip, m_node->proxy_port);
	if (m_node->transport == NULL) {
		log_error("Failed to connect to proxy");
		return FAIL;
	}

	if (m_node->transport->state == TRANSPORT_CONNECTED)
		client_connected();

	return SUCCESS;
}

result_t loop_once(void)
{
	result_t result = SUCCESS;
	int i_actor = 0;
	actor_t *actor = NULL;

	for (i_actor = 0; i_actor < MAX_ACTORS; i_actor++) {
		actor = m_node->actors[i_actor];
		if (actor != NULL && actor->enabled)
			actor->fire(actor);
	}

	for (i_actor = 0; i_actor < MAX_ACTORS && result == SUCCESS; i_actor++) {
		actor = m_node->actors[i_actor];
		if (actor != NULL && actor->enabled) {
			result = send_tokens(actor);
			if (result != SUCCESS)
				log_error("Failed to send '%s' tokens", actor->name);
		}
	}

	return result;
}

void node_run(void)
{
	// TODO: Set timeout based on active timers
	uint32_t timeout = 1;

	while (1) {
		if (wait_for_data(&m_node->transport, timeout) == SUCCESS)
			loop_once();

		if (m_node->transport == NULL ||
			(m_node->transport != NULL && m_node->transport->state == TRANSPORT_DISCONNECTED))
			stop_node(true);
	}
}

void stop_node(bool terminate)
{
	pending_msg_t *tmp_msg = NULL;
	int i = 0;

	log_debug("Stopping node");

	// TODO: If not terminating, try to reconnect actors
	// when new proxy connection is made.
	for (i = 0; i < MAX_ACTORS; i++) {
		if (m_node->actors[i] != NULL) {
			free_actor(m_node, m_node->actors[i], false);
			m_node->actors[i] = NULL;
		}
	}

	for (i = 0; i < MAX_TUNNELS; i++) {
		if (m_node->tunnels[i] != NULL) {
			free_tunnel(m_node, m_node->tunnels[i]);
			m_node->tunnels[i] = NULL;
		}
	}

	for (i = 0; i < MAX_LINKS; i++) {
		if (m_node->links[i] != NULL) {
			free_link(m_node->links[i]);
			m_node->links[i] = NULL;
		}
	}

	if (m_node->storage_tunnel != NULL) {
		free_tunnel(m_node, m_node->storage_tunnel);
		m_node->storage_tunnel = NULL;
	}

	if (m_node->proxy_link != NULL) {
		free_link(m_node->proxy_link);
		m_node->proxy_link = NULL;
	}

	if (m_node->transport != NULL) {
		free_client(m_node->transport);
		m_node->transport = NULL;
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

	if (terminate) {
		if (m_node->node_id != NULL)
			free(m_node->node_id);

		free(m_node);

		exit(EXIT_SUCCESS);
	}
}
