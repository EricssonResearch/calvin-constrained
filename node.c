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
#include "msgpuck/msgpuck.h"
#include "transport_common.h"

#ifdef USE_PERSISTENT_STORAGE
#define NODE_STATE_BUFFER_SIZE			10000
#endif

static node_t m_node;

static void node_reset(bool remove_actors)
{
	int i = 0;
	list_t *tmp_list = NULL;

	log("Resetting node");

	while (m_node.actors != NULL) {
		tmp_list = m_node.actors;
		m_node.actors = m_node.actors->next;
		if (remove_actors)
			actor_free(&m_node, (actor_t *)tmp_list->data);
		else
			actor_disconnect((actor_t *)tmp_list->data);
	}
	if (remove_actors)
		m_node.actors = NULL;

	while (m_node.tunnels != NULL) {
		tmp_list = m_node.tunnels;
		m_node.tunnels = m_node.tunnels->next;
		tunnel_free(&m_node, (tunnel_t *)tmp_list->data);
	}
	m_node.tunnels = NULL;

	while (m_node.links != NULL) {
		tmp_list = m_node.links;
		m_node.links = m_node.links->next;
		link_free(&m_node, (link_t *)tmp_list->data);
	}
	m_node.links = NULL;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		m_node.pending_msgs[i].handler = NULL;
		m_node.pending_msgs[i].msg_data = NULL;
	}
}

#ifdef USE_PERSISTENT_STORAGE
static bool node_get_state(void)
{
	result_t result = FAIL;
	char buffer[NODE_STATE_BUFFER_SIZE], *value = NULL, *array_value = NULL;
	uint32_t i = 0, value_len = 0, array_size = 0, state = 0;
	link_t *link = NULL;
	tunnel_t *tunnel = NULL;
	actor_t *actor = NULL;

	if (platform_read_node_state(buffer, NODE_STATE_BUFFER_SIZE) == SUCCESS) {
		result = decode_uint_from_map(buffer, "state", &state);
		if (result == SUCCESS)
			m_node.state = (node_state_t)state;

		result = decode_string_from_map(buffer, "id", &value, &value_len);
		if (result == SUCCESS)
			strncpy(m_node.id, value, value_len);

		result = decode_string_from_map(buffer, "name", &value, &value_len);
		if (result == SUCCESS)
			strncpy(m_node.name, value, value_len);

		result = decode_string_from_map(buffer, "uri", &value, &value_len);
		if (result == SUCCESS)
			strncpy(m_node.transport_client->uri, value, value_len);

		if (result == SUCCESS) {
			if (get_value_from_map(buffer, "links", &array_value) == SUCCESS) {
				array_size = get_size_of_array(array_value);
				for (i = 0; i < array_size; i++) {
					if (get_value_from_array(array_value, i, &value) == SUCCESS) {
						link = link_deserialize(&m_node, value);
						if (link == NULL) {
							result = FAIL;
							break;
						} else {
							if (link->is_proxy)
								m_node.proxy_link = link;
						}
					}
				}
			}
		}

		if (result == SUCCESS) {
			if (get_value_from_map(buffer, "tunnels", &array_value) == SUCCESS) {
				array_size = get_size_of_array(array_value);
				for (i = 0; i < array_size; i++) {
					if (get_value_from_array(array_value, i, &value) == SUCCESS) {
						tunnel = tunnel_deserialize(&m_node, value);
						if (tunnel == NULL) {
							result = FAIL;
							break;
						} else {
							if (tunnel->type == TUNNEL_TYPE_STORAGE)
								m_node.storage_tunnel = tunnel;
						}
					}
				}
			}
		}

		if (result == SUCCESS) {
			if (get_value_from_map(buffer, "actors", &array_value) == SUCCESS) {
				array_size = get_size_of_array(array_value);
				for (i = 0; i < array_size; i++) {
					if (get_value_from_array(array_value, i, &value) == SUCCESS) {
						actor = actor_create(&m_node, value);
						if (actor == NULL) {
							result = FAIL;
							break;
						}
					}
				}
			}
		}

		if (result == FAIL)
			node_reset(true);
	}

	return result == SUCCESS ? true : false;
}

static void node_set_state(void)
{
	char buffer[NODE_STATE_BUFFER_SIZE];
	char *tmp = buffer;
	int nbr_of_items = 0;
	list_t *item = NULL;

	tmp = mp_encode_map(tmp, 7);
	{
		tmp = encode_uint(&tmp, "state", m_node.state);
		tmp = encode_str(&tmp, "id", m_node.id, strlen(m_node.id));
		tmp = encode_str(&tmp, "name", m_node.name, strlen(m_node.name));
		tmp = encode_str(&tmp, "uri", m_node.transport_client->uri, strlen(m_node.transport_client->uri));

		nbr_of_items = list_count(m_node.links);
		tmp = encode_array(&tmp, "links", nbr_of_items);
		{
			item = m_node.links;
			while (item != NULL) {
				tmp = link_serialize((link_t *)item->data, &tmp);
				item = item->next;
			}
		}

		nbr_of_items = list_count(m_node.tunnels);
		tmp = encode_array(&tmp, "tunnels", nbr_of_items);
		{
			item = m_node.tunnels;
			while (item != NULL) {
				tmp = tunnel_serialize((tunnel_t *)item->data, &tmp);
				item = item->next;
			}
		}

		nbr_of_items = list_count(m_node.actors);
		tmp = encode_array(&tmp, "actors", nbr_of_items);
		{
			item = m_node.actors;
			while (item != NULL) {
				tmp = mp_encode_map(tmp, 1);
				tmp = actor_serialize(&m_node, (actor_t *)item->data, &tmp, true);
				item = item->next;
			}
		}
	}

	platform_write_node_state(buffer, tmp - buffer);
}
#endif

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

	log_error("Pending msg queue is full");

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

	log_error("No pending msg with id '%s'", msg_uuid);

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

	log_error("No pending msg with id '%s'", msg_uuid);

	return FAIL;
}

bool node_can_add_pending_msg(const node_t *node)
{
	int i = 0;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		if (m_node.pending_msgs[i].handler == NULL)
			return true;
	}

	return false;
}

static result_t node_setup_reply_handler(char *data, void *msg_data)
{
	uint32_t status;
	char *value = NULL;

	if (get_value_from_map(data, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(value, "status", &status) == SUCCESS) {
			if (status == 200) {
				log("Node started");
				m_node.state = NODE_STARTED;
				return SUCCESS;
			}
			log_error("Failed to setup node, status '%d'", (int)status);
		}
	}

	return FAIL;
}

result_t node_handle_token(port_t *port, const char *data, const size_t size, uint32_t sequencenbr)
{
	if (port->actor->state == ACTOR_ENABLED)
		return fifo_com_write(&port->fifo, data, size, sequencenbr);

	return FAIL;
}

void node_handle_token_reply(char *port_id, uint32_t port_id_len, port_reply_type_t reply_type, uint32_t sequencenbr)
{
	port_t *port = NULL;

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

void node_handle_message(char *buffer, size_t len)
{
	if (proto_parse_message(&m_node, buffer) == SUCCESS) {
#ifdef USE_PERSISTENT_STORAGE
		// message successfully handled == state changed -> serialize the node
		node_set_state();
#endif
	} else
		log_error("Failed to parse message");
}

node_t *node_get()
{
	return &m_node;
}

static result_t node_create(char *name, char *uri)
{
	bool created = false;

	memset(&m_node, 0, sizeof(node_t));
	if (platform_create_calvinsys(&m_node) != SUCCESS) {
		log("Failed to create calvinsys");
		return FAIL;
	}

	m_node.transport_client = transport_create(uri);
	if (m_node.transport_client == NULL)
		return FAIL;

	#ifdef USE_PERSISTENT_STORAGE
		created = node_get_state();
		if (created)
			log("Created node from state, id '%s' name '%s'", m_node.id, m_node.name);
	#endif

	if (!created) {
		gen_uuid(m_node.id, NULL);

		if (name != NULL)
			strncpy(m_node.name, name, strlen(name));
		else
			strncpy(m_node.name, "constrained", 11);

		m_node.state = NODE_DO_START;

		log("Node created, id '%s' name '%s'", m_node.id, m_node.name);
	}

	return SUCCESS;
}

static bool node_loop_once(void)
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

void node_transport_joined(transport_client_t *transport_client, char *peer_id, uint32_t peer_id_len)
{
	if (m_node.proxy_link != NULL && strncmp(m_node.proxy_link->peer_id, peer_id, peer_id_len) != 0)
		node_reset(false); // new proxy, clear links, tunnels and disconnect ports

	m_node.proxy_link = link_create(&m_node, peer_id, peer_id_len, LINK_ENABLED, true);
	if (m_node.proxy_link == NULL)
		return;

	m_node.storage_tunnel = tunnel_create(&m_node, TUNNEL_TYPE_STORAGE, TUNNEL_DO_CONNECT, peer_id, peer_id_len, NULL, 0);
	if (m_node.storage_tunnel == NULL)
		return;

	link_add_ref(m_node.proxy_link);
	tunnel_add_ref(m_node.storage_tunnel);

	m_node.state = NODE_DO_START;
}

static void node_transmit(void)
{
	list_t *tmp_list = NULL;

	if (m_node.transport_client->has_pending_tx)
		return;

	switch (m_node.state) {
	case NODE_DO_START:
		if (proto_send_node_setup(&m_node, node_setup_reply_handler) == SUCCESS)
			m_node.state = NODE_PENDING;
		break;
	case NODE_STARTED:
		tmp_list = m_node.links;
		while (tmp_list != NULL && !m_node.transport_client->has_pending_tx) {
			link_transmit(&m_node, (link_t *)tmp_list->data);
			tmp_list = tmp_list->next;
		}

		tmp_list = m_node.tunnels;
		while (tmp_list != NULL && !m_node.transport_client->has_pending_tx) {
			tunnel_transmit(&m_node, (tunnel_t *)tmp_list->data);
			tmp_list = tmp_list->next;
		}

		tmp_list = m_node.actors;
		while (tmp_list != NULL && !m_node.transport_client->has_pending_tx) {
			actor_transmit(&m_node, (actor_t *)tmp_list->data);
			tmp_list = tmp_list->next;
		}
		break;
	case NODE_PENDING:
	default:
		break;
	}
}

void node_run(char *name, char *uri)
{
	struct timeval reconnect_timeout, *timeout = NULL;

	reconnect_timeout.tv_sec = 20;
	reconnect_timeout.tv_usec = 0;

	if (node_create(name, uri) != SUCCESS) {
		log_error("Failed to create node");
		return;
	}

	while (m_node.state != NODE_STOP) {
		timeout = NULL;
		switch (m_node.transport_client->state) {
		case TRANSPORT_DISCONNECTED:
			if (transport_connect(m_node.transport_client) == SUCCESS)
				continue;
			else
				timeout = &reconnect_timeout;
			break;
		case TRANSPORT_DO_JOIN:
			transport_join(&m_node, m_node.transport_client);
			break;
		case TRANSPORT_ENABLED:
			node_loop_once();
			node_transmit();
			break;
		default:
			break;
		}
		platform_evt_wait(&m_node, timeout);
	}

	transport_disconnect(m_node.transport_client);
}
