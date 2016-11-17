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
				log("Node started and configured");
				m_node.started = true;
				return SUCCESS;
			}
		}
	}

	log_error("Failed to configure node");

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

	if (result != SUCCESS)
		log_error("Failed to parse storage reply");

	if (result == SUCCESS && status == 200) {
		m_node.storage_tunnel->state = TUNNEL_ENABLED;
		result = proto_send_node_setup(&m_node, node_setup_reply_handler);
		if (result != SUCCESS)
			log_error("Failed send node setup request");
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

	log_debug("Link created to proxy '%s'", id);

	m_node.storage_tunnel = tunnel_create(&m_node, TUNNEL_TYPE_STORAGE, TUNNEL_PENDING, id, strlen(id), NULL, 0);
	if (m_node.storage_tunnel == NULL) {
		log_error("Failed to create tunnel");
		return FAIL;
	}

	log_debug("Storage tunnel created to proxy '%s'", id);

	if (proto_send_tunnel_request(&m_node, m_node.storage_tunnel, node_storage_tunnel_reply_handler) != SUCCESS) {
		log_error("Failed to request storage tunnel");
		tunnel_free(&m_node, m_node.storage_tunnel);
		m_node.storage_tunnel = NULL;
		return FAIL;
	}

	return SUCCESS;
}

result_t node_handle_token(port_t *port, const char *data, const size_t size, uint32_t sequencenbr)
{
	log_debug("Token '%ld' received", (unsigned long)sequencenbr);
	return fifo_com_write(&port->fifo, data, size, sequencenbr);
}

void node_handle_token_reply(char *port_id, uint32_t port_id_len, port_reply_type_t reply_type, uint32_t sequencenbr)
{
	port_t *port = NULL;

	log_debug("Token reply received for port '%.*s' with sequencenbr '%ld'", (int)port_id_len, port_id, (unsigned long)sequencenbr);

	port = port_get(&m_node, port_id, port_id_len);
	if (port != NULL) {
		if (reply_type == PORT_REPLY_TYPE_ACK)
			fifo_com_commit_read(&port->fifo, sequencenbr);
		else if (reply_type == PORT_REPLY_TYPE_NACK)
			fifo_com_cancel_read(&port->fifo, sequencenbr);
		else if (reply_type == PORT_REPLY_TYPE_ABORT)
			log_error("TODO: handle ABORT");
	} else
		log_error("Token reply received for unknown port");
}

void node_handle_data(char *data, int len)
{
	transport_state_t state = transport_get_state();

	log_debug("Received data '%d'", len);

	switch (state) {
	case TRANSPORT_CONNECTED:
		node_handle_join_reply(data);
		break;
	case TRANSPORT_JOINED:
		if (proto_parse_message(&m_node, data) != SUCCESS)
			log_error("Failed to parse message");
		else
			node_loop_once();
		break;
	default:
		log_error("Received data in unkown state");
	}
}

result_t node_join_proxy(void)
{
	if (proto_send_join_request(&m_node, SERIALIZER) != SUCCESS) {
		log_error("Failed to send join request");
		return FAIL;
	}

	return SUCCESS;
}

node_t *node_get()
{
	return &m_node;
}

result_t node_create(uint32_t vid, uint32_t pid, char *name)
{
	int i = 0;

	memset(&m_node, 0, sizeof(node_t));

	m_node.started = false;
	m_node.actors = NULL;
	m_node.storage_tunnel = NULL;
	m_node.tunnels = NULL;
	m_node.proxy_link = NULL;
	m_node.links = NULL;
	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		m_node.pending_msgs[i].handler = NULL;
		m_node.pending_msgs[i].msg_data = NULL;
	}
	m_node.vid = vid;
	m_node.pid = pid;
	gen_uuid(m_node.node_id, NULL);
	m_node.schema = SCHEMA;
	m_node.name = name;
	transport_set_state(TRANSPORT_DISCONNECTED);

	log("Node created with name '%s' id '%s' vid '%ld' pid '%ld'",
		m_node.name,
		m_node.node_id,
		(unsigned long)vid,
		(unsigned long)pid);

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

result_t node_transmit(void)
{
	list_t *tmp_list = NULL;

	log_debug("node_transmit");

	if (m_node.started && transport_can_send()) {
		tmp_list = m_node.actors;
		while (tmp_list != NULL) {
			if (actor_transmit(&m_node, (actor_t *)tmp_list->data) ==  SUCCESS)
				return SUCCESS;
			tmp_list = tmp_list->next;
		}

		tmp_list = m_node.tunnels;
		while (tmp_list != NULL) {
			if (tunnel_transmit(&m_node, (tunnel_t *)tmp_list->data) ==  SUCCESS)
				return SUCCESS;
			tmp_list = tmp_list->next;
		}

		tmp_list = m_node.links;
		while (tmp_list != NULL) {
			if (link_transmit(&m_node, (link_t *)tmp_list->data) ==  SUCCESS)
				return SUCCESS;
			tmp_list = tmp_list->next;
		}
	}

	return FAIL;
}

void node_loop_once(void)
{
	list_t *tmp_list = NULL;
	actor_t *actor = NULL;

	if (m_node.started) {
		tmp_list = m_node.actors;
		while (tmp_list != NULL) {
			actor = (actor_t *)tmp_list->data;
			if (actor->state == ACTOR_ENABLED) {
				if (actor->fire(actor) == SUCCESS)
					log_debug("Fired '%s'!", actor->name);
			}
			tmp_list = tmp_list->next;
		}
	}

	node_transmit();
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

	m_node.started = false;

	while (m_node.actors != NULL) {
		tmp_list = m_node.actors;
		m_node.actors = m_node.actors->next;
		actor_free(&m_node, (actor_t *)tmp_list->data);
	}

	while (m_node.tunnels != NULL) {
		tmp_list = m_node.tunnels;
		m_node.tunnels = m_node.tunnels->next;
		tunnel_free(&m_node, (tunnel_t *)tmp_list->data);
	}
	m_node.storage_tunnel = NULL;

	while (m_node.links != NULL) {
		tmp_list = m_node.links;
		m_node.links = m_node.links->next;
		link_free(&m_node, (link_t *)tmp_list->data);
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
