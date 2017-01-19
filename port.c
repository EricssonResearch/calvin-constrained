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
#include "node.h"

port_t *port_get(node_t *node, const char *port_id, uint32_t port_id_len);

static result_t port_remove_reply_handler(char *data, void *msg_data)
{
	return SUCCESS;
}

static result_t port_get_peer_port_reply_handler(char *data, void *msg_data)
{
	port_t *port = NULL;
	char *value = NULL, *value_value = NULL, *node_id = NULL;
	char *tmp = NULL, *end = NULL;
	node_t *node = node_get();
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
		if (strncmp(port->peer_port_id, node_id, strlen(node_id)) != 0)
			strncpy(port->peer_id, node_id, strlen(node_id));
		platform_mem_free((void *)node_id);
		port->state = PORT_DO_CONNECT;
		return SUCCESS;
	}

	log_error("Failed to parse port information");

	return FAIL;
}

static result_t port_connect_reply_handler(char *data, void *msg_data)
{
	char *value = NULL;
	uint32_t status = 0;
	port_t *port = NULL;
	node_t *node = node_get();

	if (msg_data == NULL) {
		log_error("msg_data is NULL");
		return FAIL;
	}

	port = port_get(node, (char *)msg_data, strlen((char *)msg_data));
	if (port == NULL) {
		log_error("No port with id '%s'", (char *)msg_data);
		return FAIL;
	}

	if (get_value_from_map(data, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(value, "status", &status) == SUCCESS) {
			if (status == 200) {
				log_debug("Port '%s' connected", port->port_name);
				port->state = PORT_DO_ENABLE;
				return SUCCESS;
			} else if (status == 410 || status == 404) {
				log_debug("Failed to connect port %s, doing a peer look up", port->port_id);
				port->state = PORT_DO_PEER_LOOKUP;
				return SUCCESS;
			}
			log_error("Failed to connect port");
			port->state = PORT_PENDING;
		}
	}

	return FAIL;
}

static result_t port_store_reply_handler(char *data, void *msg_data)
{
	// TODO: Check result
	return SUCCESS;
}

static result_t port_disconnect_reply_handler(char *data, void *msg_data)
{
	port_t *port = NULL;
	node_t *node = node_get();

	port = port_get(node, (char *)msg_data, strlen((char *)msg_data));
	if (port == NULL) {
		log_error("No port with id '%s'", (char *)msg_data);
		return FAIL;
	}

	port->state = PORT_DISCONNECTED;

	log_debug("Port '%s' disconnected", port->port_name);

	return SUCCESS;
}

result_t add_pending_token_response(port_t *port, uint32_t sequencenbr, bool ack)
{
	pending_token_response_t *tmp = NULL, *new = NULL;

	if (platform_mem_alloc((void **)&new, sizeof(pending_token_response_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	new->sequencenbr = sequencenbr;
	new->ack = ack;
	new->next = NULL;

	if (port->pending_token_responses == NULL) {
		port->pending_token_responses = new;
	} else {
		tmp = port->pending_token_responses;
		while (tmp->next != NULL)
			tmp = tmp->next;
		tmp->next = new;
	}

	return SUCCESS;
}

port_t *port_create(node_t *node, actor_t *actor, char *obj_port, char *obj_prev_connections, port_direction_t direction)
{
	result_t result = FAIL;
	char *obj_prev_ports = NULL, *obj_prev_port = NULL, *obj_peer = NULL, *obj_queue = NULL, *obj_properties = NULL;
	char *r = obj_port, *port_id = NULL, *port_name = NULL, *routing = NULL, *peer_id = NULL, *peer_port_id = NULL;
	uint32_t nbr_peers = 0, port_id_len = 0, port_name_len = 0, routing_len = 0, peer_id_len = 0, peer_port_id_len = 0;
	port_t *port = NULL;

	if (platform_mem_alloc((void **)&port, sizeof(port_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	memset(port, 0, sizeof(port_t));

	port->direction = direction;
	port->tunnel = NULL;
	port->state = PORT_DO_CONNECT;
	port->actor = actor;
	port->pending_token_responses = NULL;

	result = decode_string_from_map(r, "id", &port_id, &port_id_len);
	if (result == SUCCESS)
		strncpy(port->port_id, port_id, port_id_len);

	if (result == SUCCESS) {
		result = decode_string_from_map(r, "name", &port_name, &port_name_len);
		if (result == SUCCESS)
			strncpy(port->port_name, port_name, port_name_len);
	}

	if (result == SUCCESS) {
		result = get_value_from_map(r, "properties", &obj_properties);
		if (result == SUCCESS) {
			result = decode_string_from_map(obj_properties, "routing", &routing, &routing_len);
			if (result == SUCCESS && (strncmp("default", routing, routing_len) != 0 && strncmp("fanout", routing, routing_len) != 0)) {
				log_error("Unsupported routing");
				result = FAIL;
			}

			result = decode_uint_from_map(obj_properties, "nbr_peers", &nbr_peers);
			if (result == SUCCESS && nbr_peers != 1) {
				log_error("Only one peer is supported");
				result = FAIL;
			}
		}
	}

	if (result == SUCCESS)
		result = get_value_from_map(r, "queue", &obj_queue);

	if (result == SUCCESS) {
		if (direction == PORT_DIRECTION_IN) {
			result = get_value_from_map(obj_prev_connections, "inports", &obj_prev_ports);

			if (result == SUCCESS)
				result = get_value_from_map(obj_prev_ports, port->port_id, &obj_prev_port);

			if (result == SUCCESS)
				result = get_value_from_array(obj_prev_port, 0, &obj_peer);

			if (result == SUCCESS)
				result = decode_string_from_array(obj_peer, 0, &peer_id, &peer_id_len);

			if (result == SUCCESS && peer_id != NULL)
				strncpy(port->peer_id, peer_id, peer_id_len);

			if (peer_id != NULL && strncmp(peer_id, node->node_id, peer_id_len) == 0) {
				log_error("TODO: Support local ports");
				result = FAIL;
			}

			if (result == SUCCESS)
				result = decode_string_from_array(obj_peer, 1, &peer_port_id, &peer_port_id_len);

			if (result == SUCCESS && peer_port_id != NULL)
				strncpy(port->peer_port_id, peer_port_id, peer_port_id_len);

			if (result == SUCCESS) {
				result = fifo_init(&port->fifo, obj_queue);
				if (result != SUCCESS)
					log_error("Failed to init fifo");
			}
		} else {
			result = get_value_from_map(obj_prev_connections, "outports", &obj_prev_ports);

			if (result == SUCCESS)
				result = get_value_from_map(obj_prev_ports, port->port_id, &obj_prev_port);

			if (result == SUCCESS)
				result = get_value_from_array(obj_prev_port, 0, &obj_peer);

			if (result == SUCCESS)
				result = decode_string_from_array(obj_peer, 0, &peer_id, &peer_id_len);

			if (result == SUCCESS && peer_id != NULL)
				strncpy(port->peer_id, peer_id, peer_id_len);

			if (peer_id != NULL && strncmp(peer_id, node->node_id, peer_id_len) == 0) {
				log_error("TODO: Support local ports");
				result = FAIL;
			}

			if (result == SUCCESS)
				result = decode_string_from_array(obj_peer, 1, &peer_port_id, &peer_port_id_len);

			if (result == SUCCESS && peer_port_id != NULL)
				strncpy(port->peer_port_id, peer_port_id, peer_port_id_len);

			if (result == SUCCESS) {
				result = fifo_init(&port->fifo, obj_queue);
				if (result != SUCCESS)
					log_error("Failed to create fifo");
			}
		}
	}

	if (result == SUCCESS) {
		log_debug("Port '%s' created", port->port_name);
		return port;
	}

	log_error("Failed to create port");
	port_free(port);

	return NULL;
}

void port_free(port_t *port)
{
	node_t *node = node_get();
	pending_token_response_t *pending_resp = NULL;

	log_debug("Freeing port '%s'", port->port_name);

	if (port->tunnel != NULL)
		tunnel_remove_ref(node, port->tunnel);

	while (port->pending_token_responses != NULL) {
		pending_resp = port->pending_token_responses;
		port->pending_token_responses = port->pending_token_responses->next;
		platform_mem_free(pending_resp);
	}

	platform_mem_free((void *)port);
}

port_t *port_get(node_t *node, const char *port_id, uint32_t port_id_len)
{
	list_t *actors = node->actors, *ports = NULL;
	actor_t *actor = NULL;

	while (actors != NULL) {
		actor = (actor_t *)actors->data;

		ports = list_get(actor->in_ports, port_id);
		if (ports != NULL)
			return (port_t *)ports->data;

		ports = list_get(actor->out_ports, port_id);
		if (ports != NULL)
			return (port_t *)ports->data;

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
		if (port->direction == direction && strncmp(port->port_name, name, strlen(name)) == 0)
			return port;
		ports = ports->next;
	}

	return NULL;
}

void port_disconnect(port_t *port)
{
	log_debug("Disconnecting port '%s'", port->port_id);
	port->state = PORT_DO_DISCONNECT;
}

result_t port_handle_connect(node_t *node, const char *port_id, uint32_t port_id_len, const char *tunnel_id, uint32_t tunnel_id_len)
{
	port_t *port = NULL;
	tunnel_t *tunnel = NULL;

	tunnel = tunnel_get_from_id(node, tunnel_id, tunnel_id_len, TUNNEL_TYPE_TOKEN);
	if (tunnel == NULL) {
		log_error("No tunnel with '%.*s'", (int)tunnel_id_len, tunnel_id);
		return FAIL;
	}

	port = port_get(node, port_id, port_id_len);
	if (port == NULL) {
		log_error("No port with '%.*s'", (int)port_id_len, port_id);
		return FAIL;
	}

	port->state = PORT_DO_ENABLE;
	port->tunnel = tunnel;
	tunnel_add_ref(port->tunnel);
	strncpy(port->peer_id, port->tunnel->link->peer_id, strlen(port->tunnel->link->peer_id));

	log_debug("Port '%s' connected", port->port_name);

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

	port->state = PORT_DISCONNECTED; // Assume other end will connect again
	port->actor->state = ACTOR_PENDING;

	if (port->tunnel != NULL) {
		tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
	}

	log_debug("Port '%s' disconnected", port->port_name);

	return SUCCESS;
}

void port_delete(port_t *port)
{
	log_debug("Deleting port '%s'", port->port_name);
	port->state = PORT_DO_DELETE;
}

result_t port_transmit(node_t *node, port_t *port)
{
	tunnel_t *tunnel = NULL;
	token_t *token = NULL;
	uint32_t sequencenbr = 0;
	pending_token_response_t *pending_resp = NULL;

	switch (port->state) {
	case PORT_DO_CONNECT:
		if (strlen(port->peer_id) > 0) {
			if (port->tunnel == NULL) {
				tunnel = tunnel_get_from_peerid(node, port->peer_id, strlen(port->peer_id), TUNNEL_TYPE_TOKEN);
				if (tunnel != NULL) {
					if (tunnel->state == TUNNEL_ENABLED) {
						port->tunnel = tunnel;
						tunnel_add_ref(tunnel);
						port->state = PORT_PENDING;
						if (proto_send_port_connect(node, port, port_connect_reply_handler) == SUCCESS)
							return SUCCESS;
						port->state = PORT_DO_CONNECT;
						tunnel_remove_ref(node, port->tunnel);
						port->tunnel = NULL;
					}
				} else {
					tunnel = tunnel_create(node, TUNNEL_TYPE_TOKEN, TUNNEL_DO_CONNECT, port->peer_id, strlen(port->peer_id), NULL, 0);
					if (tunnel == NULL)
						log_error("Failed to create tunnel");
				}
			}
		} else {
			port->state = PORT_PENDING;
			if (proto_send_get_port(node, port->peer_port_id, port_get_peer_port_reply_handler, port->port_id) == SUCCESS)
				return SUCCESS;
			port->state = PORT_DO_CONNECT;
		}
		break;
	case PORT_DO_ENABLE:
		port->state = PORT_ENABLED;
		actor_port_enabled(port->actor);
		if (proto_send_set_port(node, port, port_store_reply_handler) == SUCCESS)
			return SUCCESS;
		break;
	case PORT_ENABLED:
		if (port->actor->state == ACTOR_ENABLED) {
			if (port->direction == PORT_DIRECTION_IN) {
				if (port->pending_token_responses != NULL) {
					if (proto_send_token_reply(node, port, port->pending_token_responses->sequencenbr, port->pending_token_responses->ack) == SUCCESS) {
						pending_resp = port->pending_token_responses;
						port->pending_token_responses = port->pending_token_responses->next;
						platform_mem_free((void *)pending_resp);
						return SUCCESS;
					}
				}
			} else {
				if (fifo_tokens_available(&port->fifo, 1)) {
					fifo_com_peek(&port->fifo, &token, &sequencenbr);
					if (proto_send_token(node, port, *token, sequencenbr) == SUCCESS)
						return SUCCESS;
					fifo_com_cancel_read(&port->fifo, sequencenbr);
					return FAIL;
				}
			}
		}
		break;
	case PORT_DO_DELETE:
		port->state = PORT_PENDING;
		if (proto_send_remove_port(node, port, port_remove_reply_handler) == SUCCESS)
			return SUCCESS;
		port->state = PORT_DO_DELETE;
		break;
	case PORT_DO_DISCONNECT:
		port->state = PORT_PENDING;
		if (proto_send_port_disconnect(node, port, port_disconnect_reply_handler) == SUCCESS)
			return SUCCESS;
		port->state = PORT_DO_DISCONNECT;
		break;
	case PORT_DO_PEER_LOOKUP:
		port->state = PORT_PENDING;
		if (proto_send_get_port(node, port->peer_port_id, port_get_peer_port_reply_handler, port->port_id) == SUCCESS)
			return SUCCESS;
		port->state = PORT_DO_PEER_LOOKUP;
		break;
	default:
		break;
	}

	return FAIL;
}
