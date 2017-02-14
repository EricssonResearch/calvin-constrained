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
#include <string.h>
#include "port.h"
#include "proto.h"
#include "platform.h"
#include "msgpack_helper.h"
#include "msgpuck/msgpuck.h"
#include "node.h"

port_t *port_get(node_t *node, const char *port_id, uint32_t port_id_len);
result_t port_setup_connection(node_t *node, port_t *port, char *peer_id, uint32_t peer_id_len, char *peer_port_id, uint32_t peer_port_id_len);

static result_t port_remove_reply_handler(node_t *node, char *data, void *msg_data)
{
	port_t *port = NULL;
	char *value = NULL;
	bool status = false;

	port = port_get(node, (char *)msg_data, strlen((char *)msg_data));
	if (port == NULL) {
		log_error("No port with id '%s'", (char *)msg_data);
		return FAIL;
	}

	if (port->state != PORT_PENDING_DELETE) {
		log_error("Port '%s' in unexpected state %d", port->id, port->state);
		return FAIL;
	}

	if (get_value_from_map(data, "value", &value) == SUCCESS) {
		if (decode_bool_from_map(value, "value", &status) == SUCCESS) {
			if (status == true)
				port->state = PORT_DELETED;
				return SUCCESS;
		}
	}

	log_error("Failed to delete port '%s' from registry", port->id);
	port->state = PORT_DO_DELETE;

	return FAIL;
}

static result_t port_get_peer_port_reply_handler(node_t *node, char *data, void *msg_data)
{
	result_t result = FAIL;
	port_t *port = NULL;
	char *value = NULL, *value_value = NULL, *node_id = NULL;
	char *tmp = NULL, *end = NULL;
	uint32_t value_value_len = 0;

	if (msg_data != NULL) {
		port = port_get(node, (char *)msg_data, strlen((char *)msg_data));
		if (port == NULL) {
			log_error("No port with id '%s'", (char *)msg_data);
			return FAIL;
		}
	} else {
		log_error("Expected msg_data");
		return FAIL;
	}

	// Discard if not in expected state (connection intiated by the peer port)
	if (port->state != PORT_PENDING_PEER_LOOKUP)
		return SUCCESS;

	if (get_value_from_map(data, "value", &value) == SUCCESS) {
		if (decode_string_from_map(value, "value", &value_value, &value_value_len) == SUCCESS) {
			tmp = strstr(value_value, "\"node_id\": \"");
			if (tmp != NULL) {
				tmp += strlen("\"node_id\": \"");
				end = strstr(tmp, "\"");
				if (end != NULL) {
					if (platform_mem_alloc((void **)&node_id, end + 1 - tmp) != SUCCESS) {
						log_error("Failed to allocate memory");
						return FAIL;
					}

					if (strncpy(node_id, tmp, end - tmp) != NULL)
						node_id[end - tmp] = '\0';
				}
			}
		}
	}

	if (node_id != NULL) {
		result = port_setup_connection(node, port, node_id, strlen(node_id), port->peer_port_id, strlen(port->peer_port_id));
		platform_mem_free((void *)node_id);
	}

	return result;
}

static result_t port_connect_reply_handler(node_t *node, char *data, void *msg_data)
{
	char *value = NULL;
	uint32_t status = 0;
	port_t *port = NULL;

	if (msg_data == NULL) {
		log_error("msg_data is NULL");
		return FAIL;
	}

	port = port_get(node, (char *)msg_data, strlen((char *)msg_data));
	if (port == NULL) {
		log_error("No port with id '%s'", (char *)msg_data);
		return FAIL;
	}

	// Discard if not in expected state (connection intiated by the peer port)
	if (port->state != PORT_PENDING_CONNECT)
		return SUCCESS;

	if (get_value_from_map(data, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(value, "status", &status) == SUCCESS) {
			if (status == 200) {
				log("Port '%s' connected to remote port '%s'", port->id, port->peer_port_id);
				port->state = PORT_DO_ENABLE;
				return SUCCESS;
			}
		}
	}

	log_error("Failed to connect port '%s'", port->id);
	port->state = PORT_DO_PEER_LOOKUP;

	return FAIL;
}

static result_t port_store_reply_handler(node_t *node, char *data, void *msg_data)
{
	port_t *port = NULL;
	char *value = NULL;
	bool status = false;

	port = port_get(node, (char *)msg_data, strlen((char *)msg_data));
	if (port == NULL) {
		log_error("No port with id '%s'", (char *)msg_data);
		return FAIL;
	}

	if (port->state != PORT_PENDING_ENABLE) {
		log_error("Port '%s' in unexpected state %d", port->id, port->state);
		return FAIL;
	}

	if (get_value_from_map(data, "value", &value) == SUCCESS) {
		if (decode_bool_from_map(value, "value", &status) == SUCCESS) {
			if (status == true) {
				log("Port '%s' enabled", port->id);
				port->state = PORT_ENABLED;
				actor_port_enabled(port->actor);
				return SUCCESS;
			}
		}
	}

	log_error("Failed to store port '%s'", port->name);
	port->state = PORT_DO_ENABLE;

	return FAIL;
}

static result_t port_disconnect_reply_handler(node_t *node, char *data, void *msg_data)
{
	port_t *port = NULL;

	port = port_get(node, (char *)msg_data, strlen((char *)msg_data));
	if (port == NULL) {
		log_error("No port with id '%s'", (char *)msg_data);
		return FAIL;
	}

	if (port->state != PORT_PENDING_DISCONNECT) {
		log_error("Port '%s' in unexpected state %d", port->id, port->state);
		return FAIL;
	}

	port->state = PORT_DISCONNECTED;
	log_debug("Port '%s' disconnected", port->name);

	return SUCCESS;
}

result_t add_pending_token_response(port_t *port, uint32_t sequencenbr, bool ack)
{
	pending_token_response_t *tmp_resp = NULL, *new_resp = NULL;

	if (platform_mem_alloc((void **)&new_resp, sizeof(pending_token_response_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	new_resp->sequencenbr = sequencenbr;
	new_resp->ack = ack;
	new_resp->next = NULL;

	if (port->pending_token_responses == NULL)
		port->pending_token_responses = new_resp;
	else {
		tmp_resp = port->pending_token_responses;
		while (tmp_resp->next != NULL)
			tmp_resp = tmp_resp->next;
		tmp_resp->next = new_resp;
	}

	return SUCCESS;
}

port_t *port_create(node_t *node, actor_t *actor, char *obj_port, char *obj_prev_connections, port_direction_t direction)
{
	char *obj_prev_ports = NULL, *obj_prev_port = NULL, *obj_peer = NULL, *obj_queue = NULL, *obj_properties = NULL;
	char *r = obj_port, *port_id = NULL, *port_name = NULL, *routing = NULL, *peer_id = NULL, *peer_port_id = NULL, *tmp_value = NULL;
	uint32_t nbr_peers = 0, port_id_len = 0, port_name_len = 0, routing_len = 0, peer_id_len = 0, peer_port_id_len = 0, port_state = PORT_DISCONNECTED;
	port_t *port = NULL;

	if (decode_string_from_map(r, "id", &port_id, &port_id_len) != SUCCESS)
		return NULL;

	if (decode_string_from_map(r, "name", &port_name, &port_name_len) != SUCCESS)
		return NULL;

	if (has_key(r, "constrained_state")) {
		if (decode_uint_from_map(r, "constrained_state", &port_state) != SUCCESS)
			return NULL;
	}

	if (get_value_from_map(r, "properties", &obj_properties) != SUCCESS)
		return NULL;

	if (decode_string_from_map(obj_properties, "routing", &routing, &routing_len) != SUCCESS)
		return NULL;

	if (strncmp("default", routing, routing_len) != 0 && strncmp("fanout", routing, routing_len) != 0) {
		log_error("Unsupported routing");
		return NULL;
	}

	if (decode_uint_from_map(obj_properties, "nbr_peers", &nbr_peers) != SUCCESS)
		return NULL;

	if (nbr_peers != 1) {
		log_error("Only one peer is supported");
		return NULL;
	}

	if (get_value_from_map(r, "queue", &obj_queue) != SUCCESS)
		return NULL;

	if (get_value_from_map(obj_prev_connections,
		direction == PORT_DIRECTION_IN ? "inports" : "outports",
		&obj_prev_ports) != SUCCESS)
		return NULL;

	if (get_value_from_map_n(obj_prev_ports, port_id, port_id_len, &obj_prev_port) != SUCCESS)
		return NULL;

	if (get_value_from_array(obj_prev_port, 0, &obj_peer) != SUCCESS)
		return NULL;

	if (get_value_from_array(obj_peer, 0, &tmp_value) != SUCCESS)
		return NULL;

	if (mp_typeof(*tmp_value) == MP_STR) {
		if (decode_str(tmp_value, &peer_id, &peer_id_len) != SUCCESS)
			return NULL;
	}

	if (decode_string_from_array(obj_peer, 1, &peer_port_id, &peer_port_id_len) != SUCCESS)
		return NULL;

	if (platform_mem_alloc((void **)&port, sizeof(port_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	memset(port, 0, sizeof(port_t));

	port->direction = direction;
	port->state = (port_state_t)port_state;
	port->actor = actor;
	port->pending_token_responses = NULL;
	port->peer_port = NULL;
	port->tunnel = NULL;
	strncpy(port->id, port_id, port_id_len);
	strncpy(port->name, port_name, port_name_len);
	strncpy(port->peer_port_id, peer_port_id, peer_port_id_len);

	if (fifo_init(&port->fifo, obj_queue) != SUCCESS) {
		log_error("Failed to init fifo");
		port_free(node, port);
		return NULL;
	}

	if (port_setup_connection(node, port, peer_id, peer_id_len, port_id, port_id_len) != SUCCESS) {
		log_error("Failed setup connections");
		port_free(node, port);
		return NULL;
	}

	log("Created port '%s' with name '%s' and direction '%s'", port->id, port->name, port->direction == PORT_DIRECTION_IN ? "in" : "out");

	return port;
}

void port_free(node_t *node, port_t *port)
{
	pending_token_response_t *pending_resp = NULL;

	log("Deleting port '%s'", port->id);

	if (port->tunnel != NULL)
		tunnel_remove_ref(node, port->tunnel);

	while (port->pending_token_responses != NULL) {
		pending_resp = port->pending_token_responses;
		port->pending_token_responses = port->pending_token_responses->next;
		platform_mem_free(pending_resp);
	}

	platform_mem_free((void *)port);
}

char *port_get_peer_id(const node_t *node, port_t *port)
{
	if (port->peer_port != NULL)
		return (char *)node->id;

	if (port->tunnel != NULL)
		return port->tunnel->link->peer_id;

	log_error("Port '%s' has no peer", port->name);

	return NULL;
}

port_t *port_get(node_t *node, const char *port_id, uint32_t port_id_len)
{
	list_t *actors = node->actors;
	actor_t *actor = NULL;
	port_t *port = NULL;

	while (actors != NULL) {
		actor = (actor_t *)actors->data;

		port = (port_t *)list_get_n(actor->in_ports, port_id, port_id_len);
		if (port != NULL)
			return port;

		port = (port_t *)list_get_n(actor->out_ports, port_id, port_id_len);
		if (port != NULL)
			return port;

		actors = actors->next;
	}

	return NULL;
}

port_t *port_get_from_name(actor_t *actor, const char *name, port_direction_t direction)
{
	list_t *ports = NULL;
	port_t *port = NULL;

	if (direction == PORT_DIRECTION_IN)
		ports = actor->in_ports;
	else
		ports = actor->out_ports;

	while (ports != NULL) {
		port = (port_t *)ports->data;
		if (port->direction == direction && strncmp(port->name, name, strlen(name)) == 0)
			return port;
		ports = ports->next;
	}

	return NULL;
}

result_t port_handle_connect(node_t *node, const char *port_id, uint32_t port_id_len, const char *tunnel_id, uint32_t tunnel_id_len)
{
	port_t *port = NULL;

	port = port_get(node, port_id, port_id_len);
	if (port == NULL) {
		log_error("No port with '%.*s'", (int)port_id_len, port_id);
		return FAIL;
	}

	if (port->tunnel != NULL) {
		if (strncmp(port->tunnel->id, tunnel_id, tunnel_id_len) == 0) {
			port->state = PORT_DO_ENABLE;
			return SUCCESS;
		}
		tunnel_remove_ref(node, port->tunnel);
	}

	port->tunnel = tunnel_get_from_id(node, tunnel_id, tunnel_id_len);
	if (port->tunnel == NULL) {
		log_error("No tunnel with '%.*s'", (int)tunnel_id_len, tunnel_id);
		return FAIL;
	}

	port->state = PORT_DO_ENABLE;
	tunnel_add_ref(port->tunnel);

	log("Port '%s' connected by remote port '%s'", port->name, port->peer_port_id);

	return SUCCESS;
}

result_t port_handle_disconnect(node_t *node, const char *port_id, uint32_t port_id_len)
{
	port_t *port = NULL;

	port = port_get(node, port_id, port_id_len);
	if (port == NULL) {
		log_error("No port with '%.*s'", (int)port_id_len, port_id);
		return FAIL;
	}

	port->state = PORT_DISCONNECTED;

	if (port->tunnel != NULL) {
		tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
	}

	log("Disconnected port '%s' on actor '%s'", port->name, port->actor->name);

	return SUCCESS;
}

result_t port_setup_connection(node_t *node, port_t *port, char *peer_id, uint32_t peer_id_len, char *peer_port_id, uint32_t peer_port_id_len)
{
	port_t *peer_port = NULL;

	if (port->tunnel != NULL || port->peer_port != NULL) {
		log("Port '%s' already connected", port->id);
		return SUCCESS;
	}

	if (port->tunnel != NULL) {
		tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
	}

	peer_port = port_get(node, peer_port_id, peer_port_id_len);
	if (peer_port != NULL) {
		if (peer_port->tunnel != NULL) {
			tunnel_remove_ref(node, peer_port->tunnel);
			peer_port->tunnel = NULL;
		}
		port->peer_port = peer_port;
		port->state = PORT_DO_ENABLE;
		port->tunnel = NULL;
		peer_port->peer_port = port;
		peer_port->state = PORT_DO_ENABLE;
		log("Port '%s' connected to local port '%s'", port->id, port->peer_port_id);
		return SUCCESS;
	}

	if (peer_id != NULL) {
		port->tunnel = tunnel_get_from_peerid_and_type(node, peer_id, peer_id_len, TUNNEL_TYPE_TOKEN);
		if (port->tunnel == NULL) {
			port->tunnel = tunnel_create(node, TUNNEL_TYPE_TOKEN, TUNNEL_DO_CONNECT, peer_id, peer_id_len, NULL, 0);
			if (port->tunnel == NULL) {
				log("Failed to create a tunnel for port '%s'", port->id);
				return FAIL;
			}
		}
		tunnel_add_ref(port->tunnel);
		port->state = PORT_DO_CONNECT;
		return SUCCESS;
	}

	log("Port '%s' has no local peer and no peer_id, setting state 'PORT_DO_PEER_LOOKUP'", port->id);
	port->state = PORT_DO_PEER_LOOKUP;

	return SUCCESS;
}

void port_transmit(node_t *node, port_t *port)
{
	token_t *token = NULL;
	uint32_t sequencenbr = 0;
	pending_token_response_t *pending_resp = NULL;

	switch (port->state) {
	case PORT_DO_CONNECT:
		if (port->tunnel == NULL) {
			log_error("Port '%s' in 'PORT_DO_CONNECT' without a tunnel", port->name);
			port->state = PORT_DO_PEER_LOOKUP;
		} else {
			if (port->tunnel->state == TUNNEL_ENABLED)
				if (proto_send_port_connect(node, port, port_connect_reply_handler) == SUCCESS)
					port->state = PORT_PENDING_CONNECT;
		}
		break;
	case PORT_DO_ENABLE:
		if (proto_send_set_port(node, port, port_store_reply_handler) == SUCCESS)
			port->state = PORT_PENDING_ENABLE;
		break;
	case PORT_ENABLED:
		if (port->actor->state == ACTOR_ENABLED) {
			if (port->direction == PORT_DIRECTION_IN) {
				// send unsent token ACKs/NACKs
				if (port->pending_token_responses != NULL) {
					if (proto_send_token_reply(node, port, port->pending_token_responses->sequencenbr, port->pending_token_responses->ack) == SUCCESS) {
						pending_resp = port->pending_token_responses;
						port->pending_token_responses = port->pending_token_responses->next;
						platform_mem_free((void *)pending_resp);
					}
				}
			} else {
				// send/move token
				if (fifo_tokens_available(&port->fifo, 1)) {
					fifo_com_peek(&port->fifo, &token, &sequencenbr);
					if (port->tunnel != NULL) {
						if (proto_send_token(node, port, *token, sequencenbr) != SUCCESS)
							fifo_com_cancel_read(&port->fifo, sequencenbr);
					} else if (port->peer_port != NULL) {
						if (fifo_write(&port->peer_port->fifo, token->value, token->size) == SUCCESS)
							fifo_com_commit_read(&port->fifo, sequencenbr);
						else
							fifo_com_cancel_read(&port->fifo, sequencenbr);
					} else {
						log_error("Port '%s' is enabled without a peer", port->name);
						fifo_com_cancel_read(&port->fifo, sequencenbr);
					}
				}
			}
		}
		break;
	case PORT_DO_DELETE:
		if (proto_send_remove_port(node, port, port_remove_reply_handler) == SUCCESS)
			port->state = PORT_PENDING_DELETE;
		break;
	case PORT_DO_DISCONNECT:
		if (proto_send_port_disconnect(node, port, port_disconnect_reply_handler) == SUCCESS)
			port->state = PORT_PENDING_DISCONNECT;
		break;
	case PORT_DO_PEER_LOOKUP:
		if (proto_send_get_port(node, port->peer_port_id, port_get_peer_port_reply_handler, port->id) == SUCCESS)
			port->state = PORT_PENDING_PEER_LOOKUP;
		break;
	default:
		break;
	}
}
