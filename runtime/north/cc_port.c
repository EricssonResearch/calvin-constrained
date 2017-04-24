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
#include "cc_port.h"
#include "cc_node.h"
#include "cc_proto.h"
#include "cc_msgpack_helper.h"
#include "../../msgpuck/msgpuck.h"
#include "../south/platform/cc_platform.h"

static result_t port_setup_connection(node_t *node, port_t *port, char *peer_id, uint32_t peer_id_len);

static result_t port_remove_reply_handler(node_t *node, char *data, void *msg_data)
{
	return SUCCESS;
}

static result_t port_get_peer_port_reply_handler(node_t *node, char *data, void *msg_data)
{
	port_t *port = NULL;
	char *value = NULL, *value_value = NULL, *node_id = NULL;
	char *tmp = NULL, *end = NULL, *value_key = NULL;
	uint32_t value_value_len = 0, value_key_len = 0, status = 0;

	if (get_value_from_map(data, "value", &value) != SUCCESS)
		return FAIL;

	if (has_key(value, "status")) {
		if (decode_uint_from_map(value, "status", &status) != SUCCESS)
			return FAIL;

		if (status != 200) {
			log_error("Failed to get port info");
			return SUCCESS;
		}
	}

	if (decode_string_from_map(value, "key", &value_key, &value_key_len) != SUCCESS)
		return FAIL;

	port = port_get_from_peer_port_id(node, value_key + 5, value_key_len - 5);
	if (port == NULL)
		return SUCCESS;

	if (decode_string_from_map(value, "value", &value_value, &value_value_len) != SUCCESS)
		return FAIL;

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

	if (node_id == NULL) {
		log_error("Failed to decode message");
		return FAIL;
	}

	port_setup_connection(node, port, node_id, strlen(node_id));
	platform_mem_free((void *)node_id);

	return SUCCESS;
}

static void port_enable(port_t *port)
{
	port_set_state(port, PORT_ENABLED);
	actor_port_enabled(port->actor);
}

static result_t port_connect_reply_handler(node_t *node, char *data, void *msg_data)
{
	char *value = NULL, *value_data = NULL, *peer_port_id = NULL;
	uint32_t status = 0, peer_port_id_len = 0;
	port_t *port = NULL;

	if (get_value_from_map(data, "value", &value) != SUCCESS)
		return FAIL;

	if (decode_uint_from_map(value, "status", &status) != SUCCESS)
		return FAIL;

	if (status == 200) {
		if (get_value_from_map(value, "data", &value_data) != SUCCESS)
			return FAIL;

		if (decode_string_from_map(value_data, "port_id", &peer_port_id, &peer_port_id_len) != SUCCESS)
			return FAIL;

		port = port_get_from_peer_port_id(node, peer_port_id, peer_port_id_len);
		if (port != NULL)
			port_enable(port);
	}

	return SUCCESS;
}

static result_t port_store_reply_handler(node_t *node, char *data, void *msg_data)
{
	char *value = NULL;
	bool status = false;

	if (get_value_from_map(data, "value", &value) == SUCCESS) {
		if (decode_bool_from_map(value, "value", &status) == SUCCESS) {
			if (decode_bool_from_map(value, "value", &status) == SUCCESS) {
				if (status != true)
					log_error("Failed to store port '%s'", (char *)msg_data);
				return SUCCESS;
			}
		}
	}

	log_error("Failed to decode message");
	return FAIL;
}

static result_t port_disconnect_reply_handler(node_t *node, char *data, void *msg_data)
{
	log("Port '%s' disconnected", msg_data);
	return SUCCESS;
}

void port_set_state(port_t *port, port_state_t state)
{
	log_debug("Port '%s' state '%d' -> '%d'", port->id, port->state, state);
	port->state = state;
}

port_t *port_create(node_t *node, actor_t *actor, char *obj_port, char *obj_prev_connections, port_direction_t direction)
{
	char *obj_prev_ports = NULL, *obj_prev_port = NULL, *obj_peer = NULL, *obj_queue = NULL, *obj_properties = NULL;
	char *r = obj_port, *port_id = NULL, *port_name = NULL, *routing = NULL, *peer_id = NULL, *peer_port_id = NULL, *tmp_value = NULL;
	uint32_t nbr_peers = 0, port_id_len = 0, port_name_len = 0, routing_len = 0, peer_id_len = 0, peer_port_id_len = 0;
	port_t *port = NULL;

	if (decode_string_from_map(r, "id", &port_id, &port_id_len) != SUCCESS)
		return NULL;

	if (decode_string_from_map(r, "name", &port_name, &port_name_len) != SUCCESS)
		return NULL;

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
	port->state = PORT_DISCONNECTED;
	port->actor = actor;
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

	if (peer_id != NULL) {
		port->tunnel = tunnel_get_from_peerid_and_type(node, peer_id, peer_id_len, TUNNEL_TYPE_TOKEN);
		if (port->tunnel != NULL) {
			port_set_state(port, PORT_PENDING);
			actor_port_enabled(actor);
		}
		tunnel_add_ref(port->tunnel);
	}

	if (node->transport_client != NULL) {
		if (proto_send_set_port(node, port, port_store_reply_handler) != SUCCESS) {
			log_error("Failed to store port");
			port_free(node, port);
			return NULL;
		}

		if (port_setup_connection(node, port, peer_id, peer_id_len) != SUCCESS) {
			log_error("Failed setup connections");
			port_free(node, port);
			return NULL;
		}
	}

	log("Created port '%s' with name '%s' and direction '%s'", port->id, port->name, port->direction == PORT_DIRECTION_IN ? "in" : "out");

	return port;
}

void port_free(node_t *node, port_t *port)
{
	log("Deleting port '%s'", port->id);

	if (proto_send_remove_port(node, port, port_remove_reply_handler) != SUCCESS)
		log_error("Failed to remove port '%s'", port->id);

	if (port->tunnel != NULL)
		tunnel_remove_ref(node, port->tunnel);

	platform_mem_free((void *)port);
}

char *port_get_peer_id(const node_t *node, port_t *port)
{
	if (port->peer_port != NULL)
		return (char *)node->id;

	if (port->tunnel != NULL)
		return port->tunnel->link->peer_id;

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

port_t *port_get_from_peer_port_id(struct node_t *node, const char *peer_port_id, uint32_t peer_port_id_len)
{
	list_t *actors = node->actors;
	actor_t *actor = NULL;
	list_t *ports = NULL;
	port_t *port = NULL;

	while (actors != NULL) {
		actor = (actor_t *)actors->data;

		ports = actor->in_ports;
		while (ports != NULL) {
			port = (port_t *)ports->data;
			if (strncmp(port->peer_port_id, peer_port_id, peer_port_id_len) == 0)
				return port;
			ports = ports->next;
		}

		ports = actor->out_ports;
		while (ports != NULL) {
			port = (port_t *)ports->data;
			if (strncmp(port->peer_port_id, peer_port_id, peer_port_id_len) == 0)
				return port;
			ports = ports->next;
		}
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
			port_set_state(port, PORT_ENABLED);
			actor_port_enabled(port->actor);
			return SUCCESS;
		}
		tunnel_remove_ref(node, port->tunnel);
	}

	port->tunnel = tunnel_get_from_id(node, tunnel_id, tunnel_id_len);
	if (port->tunnel == NULL) {
		log_error("No tunnel with '%.*s'", (int)tunnel_id_len, tunnel_id);
		return FAIL;
	}

	tunnel_add_ref(port->tunnel);
	port_set_state(port, PORT_ENABLED);
	actor_port_enabled(port->actor);
	log("Port '%s' connected by remote", port->id);

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

	fifo_cancel(&port->fifo);
	port_set_state(port, PORT_DISCONNECTED);
	actor_port_disconnected(port->actor);

	if (port->tunnel != NULL) {
		tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
	}

	log("Port '%s' disconnected by remote", port->id);

	return SUCCESS;
}

static result_t port_setup_connection(node_t *node, port_t *port, char *peer_id, uint32_t peer_id_len)
{
	port_t *peer_port = NULL;

	// handle pending tunnel
	if (port->tunnel != NULL) {
		if (port->tunnel->state == TUNNEL_PENDING)
			return SUCCESS;
		else if (port->tunnel->state == TUNNEL_ENABLED) {
			if (proto_send_port_connect(node, port, port_connect_reply_handler) == SUCCESS) {
				port_set_state(port, PORT_PENDING);
				return SUCCESS;
			}
		}
		tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
		port_set_state(port, PORT_DISCONNECTED);
	}

	// handle local connection
	peer_port = port_get(node, port->peer_port_id, strlen(port->peer_port_id));
	if (peer_port != NULL) {
		if (peer_port->tunnel != NULL) {
			tunnel_remove_ref(node, peer_port->tunnel);
			peer_port->tunnel = NULL;
		}
		port->peer_port = peer_port;
		port_set_state(port, PORT_ENABLED);
		port->tunnel = NULL;
		peer_port->peer_port = port;
		port_set_state(peer_port, PORT_ENABLED);
		log("Port '%s' connected to local port '%s'", port->id, port->peer_port_id);
		return SUCCESS;
	}

	// get/create a tunnel
	if (peer_id != NULL) {
		port->tunnel = tunnel_get_from_peerid_and_type(node, peer_id, peer_id_len, TUNNEL_TYPE_TOKEN);
		if (port->tunnel == NULL) {
			port->tunnel = tunnel_create(node, TUNNEL_TYPE_TOKEN, TUNNEL_DISCONNECTED, peer_id, peer_id_len, NULL, 0);
			if (port->tunnel == NULL) {
				log("Failed to create a tunnel for port '%s'", port->id);
				return FAIL;
			}
		}
		port->state = PORT_PENDING;
		tunnel_add_ref(port->tunnel);
		return SUCCESS;
	}

	// lookup peer
	if (port->state == PORT_DISCONNECTED) {
		if (proto_send_get_port(node, port->peer_port_id, port_get_peer_port_reply_handler, port->id) == SUCCESS) {
			log("Doing peer lookup for port '%s' with peer port '%s'", port->id, port->peer_port_id);
			port_set_state(port, PORT_PENDING);
			return SUCCESS;
		}
	}

	return FAIL;
}

void port_disconnect(node_t *node, port_t *port)
{
	if (port->tunnel != NULL) {
		if (proto_send_port_disconnect(node, port, port_disconnect_reply_handler) != SUCCESS)
			log_error("Failed to disconnect port");
		tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
	}

	port->peer_port = NULL;
	port_set_state(port, PORT_DISCONNECTED);
	fifo_cancel(&port->fifo);
}

void port_transmit(node_t *node, port_t *port)
{
	token_t *token = NULL;
	uint32_t sequencenbr = 0;

	if (port->state == PORT_ENABLED) {
		if (port->actor->state == ACTOR_ENABLED) {
			if (port->direction == PORT_DIRECTION_OUT) {
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
	} else
		port_setup_connection(node, port, NULL, 0);
}
