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

static result_t port_connected(node_t *node, char *port_peer_id);

static port_t *get_port(node_t *node, const char *port_id)
{
	port_t *port = NULL;

	port = get_inport(node, port_id);
	if (port != NULL)
		return port;

	port = get_outport(node, port_id);
	if (port != NULL)
		return port;

	return port;
}

static void add_port(port_t **head, port_t *port)
{
	port_t *tmp = NULL;

	if (*head == NULL)
		*head = port;
	else {
		tmp = *head;
		while (tmp->next != NULL)
			tmp = tmp->next;
		tmp = port;
	}

	log_debug("Added port '%s'", port->port_id);
}

result_t create_port(node_t *node, actor_t *actor, port_t **port, port_t **head, char *obj_port, char *obj_prev_connections, port_direction_t direction)
{
	result_t result = FAIL;
	char *obj_prev_ports = NULL, *obj_prev_port = NULL, *obj_peer = NULL, *obj_queue = NULL, *obj_properties = NULL;
	char *routing = NULL, *r = obj_port;
	uint32_t nbr_peers = 0;

	*port = (port_t *)malloc(sizeof(port_t));
	if (*port == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	(*port)->port_id = NULL;
	(*port)->peer_id = NULL;
	(*port)->peer_port_id = NULL;
	(*port)->port_name = NULL;
	(*port)->direction = direction;
	(*port)->tunnel = NULL;
	(*port)->fifo = NULL;
	(*port)->next = NULL;
	(*port)->is_local = true;
	(*port)->local_connection = NULL;
	(*port)->state = PORT_DISCONNECTED;
	(*port)->actor = actor;

	result = decode_string_from_map(&r, "id", &(*port)->port_id);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "name", &(*port)->port_name);

	if (result == SUCCESS) {
		result = get_value_from_map(&r, "properties", &obj_properties);
		if (result == SUCCESS) {
			result = decode_string_from_map(&obj_properties, "routing", &routing);
			if (result == SUCCESS && (strcmp("default", routing) != 0 && strcmp("fanout", routing) != 0)) {
				log_error("Unsupported routing");
				result = FAIL;
			}

			if (routing != NULL)
				free(routing);

			result = decode_uint_from_map(&obj_properties, "nbr_peers", &nbr_peers);
			if (result == SUCCESS && nbr_peers != 1) {
				log_error("Only one peer is supported");
				result = FAIL;
			}
		}
	}

	if (result == SUCCESS)
		result = get_value_from_map(&r, "queue", &obj_queue);

	if (result == SUCCESS) {
		if (direction == IN) {
			result = get_value_from_map(&obj_prev_connections, "inports", &obj_prev_ports);

			if (result == SUCCESS)
				result = get_value_from_map(&obj_prev_ports, (*port)->port_id, &obj_prev_port);

			if (result == SUCCESS)
				result = get_value_from_array(&obj_prev_port, 0, &obj_peer);

			if (result == SUCCESS)
				result = decode_string_from_array(&obj_peer, 0, &(*port)->peer_id);

			if ((*port)->peer_id != NULL && strcmp((*port)->peer_id, node->node_id) == 0)
				(*port)->is_local = true;
			else
				(*port)->is_local = false;

			if (result == SUCCESS)
				result = decode_string_from_array(&obj_peer, 1, &(*port)->peer_port_id);

			if (result == SUCCESS) {
				result = create_fifo(&(*port)->fifo, obj_queue);
				if (result != SUCCESS)
					log_error("Failed to create fifo");
			}
		} else {
			result = get_value_from_map(&obj_prev_connections, "outports", &obj_prev_ports);

			if (result == SUCCESS)
				result = get_value_from_map(&obj_prev_ports, (*port)->port_id, &obj_prev_port);

			if (result == SUCCESS)
				result = get_value_from_array(&obj_prev_port, 0, &obj_peer);

			if (result == SUCCESS)
				result = decode_string_from_array(&obj_peer, 0, &(*port)->peer_id);

			if ((*port)->peer_id != NULL && strcmp((*port)->peer_id, node->node_id) == 0)
				(*port)->is_local = true;
			else
				(*port)->is_local = false;

			if (result == SUCCESS)
				result = decode_string_from_array(&obj_peer, 1, &(*port)->peer_port_id);

			if (result == SUCCESS) {
				result = create_fifo(&(*port)->fifo, obj_queue);
				if (result != SUCCESS)
					log_error("Failed to create fifo");
			}
		}
	}

	if (result == SUCCESS) {
		log_debug("Port '%s' created", (*port)->port_id);
		add_port(head, (*port));
	} else
		free_port(node, (*port), false);

	return result;
}

static result_t remove_port_reply_handler(char *data, void *msg_data)
{
	log_debug("TODO: remove_port_reply_handler does nothing");
	return SUCCESS;
}

void free_port(node_t *node, port_t *port, bool remove_from_storage)
{
	if (port != NULL) {
		log_debug("Freeing port '%s'", port->port_id);
		if (remove_from_storage && node != NULL) {
			if (send_remove_port(node, port, remove_port_reply_handler) != SUCCESS)
				log_error("Failed to send remove port request");
		}
		if (port->tunnel != NULL)
			tunnel_client_disconnected(node, port->tunnel);
		free(port->port_id);
		free(port->port_name);
		free(port->peer_id);
		free(port->peer_port_id);
		free_fifo(port->fifo);
		free(port);
	}
}

static result_t set_port_reply_handler(char *data, void *msg_data)
{
	log_debug("TODO: set_port_reply_handler does nothing");
	return SUCCESS;
}

static result_t store_port(node_t *node, actor_t *actor, port_t *port)
{
	result_t result = FAIL;

	result = send_set_port(node, actor, port, set_port_reply_handler);
	if (result != SUCCESS)
		log_error("Failed to store port '%s'", port->port_id);

	return result;
}

static result_t get_peer_port_reply_handler(char *data, void *msg_data)
{
	port_t *port = NULL;
	char *value = NULL, *value_value = NULL, *node_id = NULL;
	char *tmp = NULL, *end = NULL;
	tunnel_t *tunnel = NULL;
	result_t result = FAIL;
	node_t *node = get_node();

	if (msg_data != NULL) {
		port = get_port(node, (char *)msg_data);
		if (port == NULL) {
			log_error("No port with id '%s'", (char *)msg_data);
			return FAIL;
		}
	} else {
		log_error("Expected msg_data");
		return FAIL;
	}

	if (get_value_from_map(&data, "value", &value) == SUCCESS) {
		if (decode_string_from_map(&value, "value", &value_value) == SUCCESS) {
			tmp = strstr(value_value, "\"node_id\": \"");
			if (tmp != NULL) {
				tmp += strlen("\"node_id\": \"");
				end = strstr(tmp, "\"");
				if (end != NULL) {
					node_id = malloc(end + 1 - tmp);
					if (node_id == NULL) {
						log_error("Failed to allocate memory");
						return FAIL;
					}
					if (strncpy(node_id, tmp, end - tmp) != NULL) {
						node_id[end - tmp] = '\0';
						port->peer_id = node_id;
						tunnel = get_token_tunnel_from_peerid(node_id);
						if (tunnel != NULL) {
							port->tunnel = tunnel;
							if (tunnel->state == TUNNEL_CONNECTED) {
								result = connect_port(node, port, tunnel);
								if (result != SUCCESS)
									log_error("Failed to connect port '%s'", port->port_id);
							}
						} else {
							if (strcmp(node_id, node->proxy_node_id) == 0)
								result = request_tunnel(port->peer_id, TOKEN_TUNNEL, token_tunnel_reply_handler);
							else
								result = send_route_request(node, port->peer_id, route_request_handler);
						}
					}
				}
			}
			free(value_value);
		}
	}

	return result;
}

result_t connect_ports(node_t *node, actor_t *actor, port_t *ports)
{
	result_t result = SUCCESS;
	tunnel_t *tunnel = NULL;
	port_t *port = NULL;

	port = ports;
	while (port != NULL && result == SUCCESS) {
		if (!port->is_local) {
			tunnel = get_token_tunnel_from_peerid(port->peer_id);
			if (tunnel != NULL) {
				port->tunnel = tunnel;
				if (tunnel->state == TUNNEL_CONNECTED) {
					result = connect_port(node, port, tunnel);
					if (result != SUCCESS) {
						log_error("Failed to connect port '%s'", port->port_id);
						break;
					}
				}
			} else {
				if (port->peer_id != NULL) {
					if (strcmp(port->peer_id, node->proxy_node_id) == 0)
						result = request_tunnel(port->peer_id, TOKEN_TUNNEL, token_tunnel_reply_handler);
					else
						result = send_route_request(node, port->peer_id, route_request_handler);
				} else
					result = send_get_port(node, port->peer_port_id, get_peer_port_reply_handler, port->port_id);
			}
		} else
			port->state = PORT_CONNECTED;

		if (result == SUCCESS)
			result = store_port(node, actor, port);

		port = port->next;
	}

	return result;
}

port_t *get_inport(node_t *node, const char *port_id)
{
	int i_actor = 0;
	port_t *port = NULL;

	for (i_actor = 0; i_actor < MAX_ACTORS; i_actor++) {
		if (node->actors[i_actor] != NULL) {
			port = node->actors[i_actor]->inports;
			while (port != NULL) {
				if (strcmp(port->port_id, port_id) == 0)
					return port;
				port = port->next;
			}
		}
	}

	return NULL;
}

port_t *get_outport(node_t *node, const char *port_id)
{
	int i_actor = 0;
	port_t *port = NULL;

	for (i_actor = 0; i_actor < MAX_ACTORS; i_actor++) {
		if (node->actors[i_actor] != NULL) {
			port = node->actors[i_actor]->outports;
			while (port != NULL) {
				if (strcmp(port->port_id, port_id) == 0)
					return port;
				port = port->next;
			}
		}
	}

	return NULL;
}

static result_t port_connect_reply_handler(char *data, void *msg_data)
{
	result_t result = FAIL;
	char *value = NULL;
	uint32_t status = 0;
	port_t *port = (port_t *)msg_data;
	node_t *node = get_node();

	log_debug("port_connect_reply_handler");

	if (get_value_from_map(&data, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(&value, "status", &status) == SUCCESS) {
			if (status == 200)
				result = port_connected(node, port->peer_port_id);
			else {
				log_debug("Failed to connect port %s, getting port info and retrying", port->port_id);
				result = send_get_port(node, port->peer_port_id, get_peer_port_reply_handler, port->port_id);
			}
		}
	}

	return result;
}

result_t connect_port(node_t *node, port_t *port, tunnel_t *tunnel)
{
	result_t result = SUCCESS;

	port->tunnel = tunnel;

	result = send_port_connect(node, port, port_connect_reply_handler);
	if (result != SUCCESS)
		log_error("Failed to connect port '%s'", port->port_name);

	return result;
}

static result_t port_disconnect_reply_handler(char *data, void *msg_data)
{
	log_debug("TODO: port_disconnect_reply_handler does nothing");
	return SUCCESS;
}

result_t disconnect_port(node_t *node, port_t *port)
{
	result_t result = SUCCESS;

	disable_actor(port->actor);

	if (port->state == PORT_CONNECTED) {
		result = send_port_disconnect(node, port, port_disconnect_reply_handler);
		if (result != SUCCESS)
			log_error("Failed to send port disconnect for port '%s'", port->port_name);

		port->state = PORT_DISCONNECTED;
		tunnel_client_disconnected(node, port->tunnel);
		port->tunnel = NULL;
	}

	return result;
}

result_t handle_port_disconnect(node_t *node, const char *port_id)
{
	port_t *port = NULL;

	port = get_port(node, port_id);
	if (port != NULL) {
		port->state = PORT_DISCONNECTED;
		disable_actor(port->actor);
		tunnel_client_disconnected(node, port->tunnel);
		port->tunnel = NULL;
		return SUCCESS;
	}

	log_error("Disconnected port '%s'", port_id);

	return FAIL;
}

static result_t port_connected(node_t *node, char *port_peer_id)
{
	int i_actor = 0;
	port_t *port = NULL;

	for (i_actor = 0; i_actor < MAX_ACTORS; i_actor++) {
		if (node->actors[i_actor] != NULL) {
			port = node->actors[i_actor]->inports;
			while (port != NULL) {
				if (strcmp(port->peer_port_id, port_peer_id) == 0) {
					log_debug("Port '%s' connected", port->port_id);
					port->state = PORT_CONNECTED;
					tunnel_client_connected(port->tunnel);
					enable_actor(node->actors[i_actor]);
					store_port(node, port->actor, port);
					return SUCCESS;
				}
				port = port->next;
			}

			port = node->actors[i_actor]->outports;
			while (port != NULL) {
				if (strcmp(port->peer_port_id, port_peer_id) == 0) {
					log_debug("Port '%s' connected", port->port_id);
					port->state = PORT_CONNECTED;
					tunnel_client_connected(port->tunnel);
					enable_actor(node->actors[i_actor]);
					store_port(node, port->actor, port);
					return SUCCESS;
				}
				port = port->next;
			}
		}
	}

	log_error("No port with peer port id '%s'", port_peer_id);

	return FAIL;
}

result_t handle_port_connect(node_t *node, const char *port_id, const char *tunnel_id)
{
	port_t *port = NULL;
	tunnel_t *tunnel = NULL;

	tunnel = get_token_tunnel(tunnel_id);
	if (tunnel == NULL) {
		log_error("Failed to connect port '%s', no tunnel with id '%s'.", port_id, tunnel_id);
		return FAIL;
	}

	port = get_inport(node, port_id);
	if (port != NULL) {
		port->state = PORT_CONNECTED;
		port->tunnel = tunnel;
		tunnel_client_connected(tunnel);
		port->peer_id = strdup(tunnel->peer_id);
		enable_actor(port->actor);
		store_port(node, port->actor, port);
		return SUCCESS;
	}

	port = get_outport(node, port_id);
	if (port != NULL) {
		port->state = PORT_CONNECTED;
		port->tunnel = tunnel;
		tunnel_client_connected(port->tunnel);
		port->peer_id = strdup(tunnel->peer_id);
		enable_actor(port->actor);
		store_port(node, port->actor, port);
		return SUCCESS;
	}

	log_error("No port with id '%s'", port_id);

	return FAIL;
}
