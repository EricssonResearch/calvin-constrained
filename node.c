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

static node_t m_node;

result_t node_add_pending_msg(char *msg_uuid, uint32_t msg_uuid_len, result_t (*handler)(char *data, void *msg_data), void *msg_data)
{
	int i = 0;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		if (m_node.pending_msgs[i].handler == NULL) {
			strncpy(m_node.pending_msgs[i].msg_uuid, msg_uuid, msg_uuid_len);
			m_node.pending_msgs[i].handler = handler;
			m_node.pending_msgs[i].msg_data = msg_data;
			return SUCCESS;
		}
	}

	log_error("Pending message queue is full");

	return FAIL;
}

result_t node_remove_pending_msg(char *msg_uuid, uint32_t msg_uuid_len)
{
	int i = 0;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		if (m_node.pending_msgs[i].handler != NULL) {
			if (strncmp(m_node.pending_msgs[i].msg_uuid, msg_uuid, msg_uuid_len) == 0) {
				m_node.pending_msgs[i].handler = NULL;
				m_node.pending_msgs[i].msg_data = NULL;
				return SUCCESS;
			}
		}
	}

	log_error("No pending message with id '%s'", msg_uuid);

	return FAIL;
}

result_t node_get_pending_msg(const char *msg_uuid, uint32_t msg_uuid_len, pending_msg_t *pending_msg)
{
	int i = 0;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		if (m_node.pending_msgs[i].handler != NULL) {
			if (strncmp(m_node.pending_msgs[i].msg_uuid, msg_uuid, msg_uuid_len) == 0) {
				*pending_msg = m_node.pending_msgs[i];
				return SUCCESS;
			}
		}
	}

	log_error("No pending message with id '%s'", msg_uuid);

	return FAIL;
}

bool node_can_add_pending_msg(const node_t *node)
{
	int i = 0;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		if (m_node.pending_msgs[i].handler == NULL)
			return true;
	}

	log_error("Cannot add pending msg");

	return false;
}

static result_t node_setup_reply_handler(char *data, void *msg_data)
{
	uint32_t status;
	char *value = NULL;

	if (get_value_from_map(data, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(value, "status", &status) == SUCCESS) {
			if (status == 200) {
				log("Node started with proxy '%s'", m_node.proxy_link->peer_id);
				m_node.state = NODE_STARTED;
				return SUCCESS;
			}
			log_error("Failed to setup node, status '%d'", (int)status);
		}
	}

	return FAIL;
}

static result_t node_storage_tunnel_reply_handler(char *data, void *msg_data)
{
	uint32_t status, len = 0;
	char *value = NULL, *tunnel_id = NULL, *data_value = NULL;
	result_t result = FAIL;

	result = get_value_from_map(data, "value", &value);

	if (result == SUCCESS)
		result = get_value_from_map(value, "data", &data_value);

	if (result == SUCCESS)
		result = decode_string_from_map(data_value, "tunnel_id", &tunnel_id, &len);

	if (result == SUCCESS)
		result = decode_uint_from_map(value, "status", &status);

	if (result == SUCCESS && status == 200) {
		log("Storage tunnel '%s' connected to '%s'", m_node.storage_tunnel->id, m_node.storage_tunnel->link->peer_id);
		m_node.state = NODE_DO_CONFIGURE;
	} else {
		log_error("Failed to connect storage tunnel, retrying");
		m_node.state = NODE_DO_CONNECT_STORAGE_TUNNEL;
	}

	return result;
}

static result_t node_handle_join_reply(char *data)
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

	transport_set_state(TRANSPORT_JOINED);

	m_node.proxy_link = link_create(&m_node, id, strlen(id), LINK_ENABLED);
	if (m_node.proxy_link == NULL) {
		log_error("Failed to create link to node '%s'", id);
		return FAIL;
	}

	link_add_ref(m_node.proxy_link);

	m_node.storage_tunnel = tunnel_create(&m_node, TUNNEL_TYPE_STORAGE, TUNNEL_PENDING, id, strlen(id), NULL, 0);
	if (m_node.storage_tunnel == NULL) {
		link_remove_ref(&m_node, m_node.proxy_link);
		log_error("Failed to create tunnel");
		return FAIL;
	}

	tunnel_add_ref(m_node.storage_tunnel);
	m_node.state = NODE_DO_CONNECT_STORAGE_TUNNEL;

	return SUCCESS;
}

result_t node_handle_token(port_t *port, const char *data, const size_t size, uint32_t sequencenbr)
{
	result_t result = FAIL;

	result = fifo_com_write(&port->fifo, data, size, sequencenbr);
	if (result == SUCCESS)
		node_loop_once();

	return result;
}

void node_handle_token_reply(char *port_id, uint32_t port_id_len, port_reply_type_t reply_type, uint32_t sequencenbr)
{
	port_t *port = NULL;

	port = port_get(&m_node, port_id, port_id_len);
	if (port != NULL) {
		if (reply_type == PORT_REPLY_TYPE_ACK) {
			fifo_com_commit_read(&port->fifo, sequencenbr);
			node_loop_once();
		} else if (reply_type == PORT_REPLY_TYPE_NACK)
			fifo_com_cancel_read(&port->fifo, sequencenbr);
		else if (reply_type == PORT_REPLY_TYPE_ABORT)
			log_error("TODO: handle ABORT");
	} else
		log_error("Token reply received for unknown port");
}

void node_handle_data(char *data, int len)
{
	transport_state_t state = transport_get_state();

	switch (state) {
	case TRANSPORT_CONNECTED:
		if (node_handle_join_reply(data) != SUCCESS)
			m_node.state = NODE_DO_JOIN;
		break;
	case TRANSPORT_JOINED:
		if (proto_parse_message(&m_node, data) != SUCCESS)
			log_error("Failed to parse message");
		break;
	default:
		log_error("Received data in unkown state");
	}

	node_transmit();
}

node_t *node_get()
{
	return &m_node;
}

result_t node_create(char *name, char *capabilities)
{
	int i = 0;

	memset(&m_node, 0, sizeof(node_t));

	m_node.state = NODE_DO_JOIN;
	m_node.actors = NULL;
	m_node.storage_tunnel = NULL;
	m_node.tunnels = NULL;
	m_node.proxy_link = NULL;
	m_node.links = NULL;
	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		m_node.pending_msgs[i].handler = NULL;
		m_node.pending_msgs[i].msg_data = NULL;
	}
	gen_uuid(m_node.id, NULL);
	m_node.name = name;
	m_node.capabilities = capabilities;
	transport_set_state(TRANSPORT_DISCONNECTED);

	log("Node created with name '%s', id '%s' and capabilities '%s'",
		m_node.name,
		m_node.id,
		m_node.capabilities);

	return SUCCESS;
}

result_t node_start(const char *ssdp_iface, const char *proxy_iface, const int proxy_port)
{
	if (transport_start(ssdp_iface, proxy_iface, proxy_port) != SUCCESS) {
		log_error("Failed to start transport");
		return FAIL;
	}

	return SUCCESS;
}

void node_transmit(void)
{
	list_t *tmp_list = NULL;

	if (!transport_can_transmit())
		return;

	switch (m_node.state) {
	case NODE_DO_JOIN:
		if (proto_send_join_request(&m_node, SERIALIZER) == SUCCESS)
			m_node.state = NODE_PENDING;
		else
			log_error("Failed to send join request");
		break;
	case NODE_DO_CONNECT_STORAGE_TUNNEL:
		if (proto_send_tunnel_request(&m_node, m_node.storage_tunnel, node_storage_tunnel_reply_handler) == SUCCESS)
			m_node.state = NODE_PENDING;
		else
			log_error("Failed to request storage tunnel");
		break;
	case NODE_DO_CONFIGURE:
		if (proto_send_node_setup(&m_node, node_setup_reply_handler) == SUCCESS)
			m_node.state = NODE_PENDING;
		else
			log_error("Failed send node setup request");
		break;
	case NODE_STARTED:
		tmp_list = m_node.links;
		while (tmp_list != NULL && transport_can_transmit()) {
			link_transmit(&m_node, (link_t *)tmp_list->data);
			tmp_list = tmp_list->next;
		}

		tmp_list = m_node.tunnels;
		while (tmp_list != NULL && transport_can_transmit()) {
			tunnel_transmit(&m_node, (tunnel_t *)tmp_list->data);
			tmp_list = tmp_list->next;
		}

		tmp_list = m_node.actors;
		while (tmp_list != NULL && transport_can_transmit()) {
			actor_transmit(&m_node, (actor_t *)tmp_list->data);
			tmp_list = tmp_list->next;
		}
		break;
	case NODE_PENDING:
	default:
		break;
	}
}

bool node_loop_once(void)
{
	bool did_fire = false;
	list_t *tmp_list = NULL;
	actor_t *actor = NULL;

	if (m_node.state == NODE_STARTED) {
		tmp_list = m_node.actors;
		while (tmp_list != NULL) {
			actor = (actor_t *)tmp_list->data;
			if (actor->state == ACTOR_ENABLED) {
				if (actor->fire(actor)) {
					log("Fired '%s'", actor->name);
					did_fire = true;
				}
			}
			tmp_list = tmp_list->next;
		}
	}

	return did_fire;
}

void node_run(void)
{
	uint32_t timeout = 60;

	while (1) {
		if (transport_select(timeout) != SUCCESS) {
			log_error("Failed to receive data");
			node_stop(true);
			return;
		}
	}
}

void node_stop(bool terminate)
{
	int i = 0;
	list_t *tmp_list = NULL;

	log("Stopping node");

	proto_send_remove_node(&m_node, NULL);

	m_node.state = NODE_DO_JOIN;

	while (m_node.actors != NULL) {
		tmp_list = m_node.actors;
		m_node.actors = m_node.actors->next;
		actor_free(&m_node, (actor_t *)tmp_list->data);
	}

	while (m_node.tunnels != NULL) {
		tmp_list = m_node.tunnels;
		m_node.tunnels = m_node.tunnels->next;
		tunnel_remove_ref(&m_node, (tunnel_t *)tmp_list->data);
	}
	m_node.storage_tunnel = NULL;

	while (m_node.links != NULL) {
		tmp_list = m_node.links;
		m_node.links = m_node.links->next;
		link_remove_ref(&m_node, (link_t *)tmp_list->data);
	}
	m_node.proxy_link = NULL;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		m_node.pending_msgs[i].handler = NULL;
		m_node.pending_msgs[i].msg_data = NULL;
	}

	transport_stop();

	if (terminate)
		exit(EXIT_SUCCESS);
}
