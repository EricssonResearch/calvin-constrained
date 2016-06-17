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

void add_port(port_t **head, port_t *port)
{
	port_t *tmp = NULL;

	if (*head == NULL) {
		*head = port;
	} else {
		tmp = *head;
		while (tmp->next != NULL) {
			tmp = tmp->next;
		}
		tmp = port;
	}

	log_debug("Added port '%s'", port->port_id);
}

result_t create_port(port_t **port, port_t **head, char *obj_port, char *obj_prev_connections, port_direction_t direction)
{
	result_t result = FAIL;
	char *obj_prev_ports = NULL, *obj_prev_port = NULL, *obj_peer = NULL, *obj_queue = NULL, *obj_properties = NULL;
	char *routing = NULL, *r = obj_port;
	uint32_t nbr_peers = 0;

	*port = (port_t*)malloc(sizeof(port_t));
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

	result = decode_string_from_map(&r, "id", &(*port)->port_id);

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "name", &(*port)->port_name);
	}

	if (result == SUCCESS) {
		result = get_value_from_map(&r, "properties", &obj_properties);
		if (result == SUCCESS) {
			result = decode_string_from_map(&obj_properties, "routing", &routing);
			if (result == SUCCESS && (strcmp("default", routing) != 0 && strcmp("fanout", routing) != 0)) {
				log_error("Unsupported routing");
				result = FAIL;
			}

			if (routing != NULL) {
				free(routing);
			}

			result = decode_uint_from_map(&obj_properties, "nbr_peers", &nbr_peers);
			if (result == SUCCESS && nbr_peers != 1) {
				log_error("Only one peer is supported");
				result = FAIL;
			}
		}
	}

	if (result == SUCCESS) {
		result = get_value_from_map(&r, "queue", &obj_queue);
	}

	if (result == SUCCESS) {
		if (direction == IN) {
			result = get_value_from_map(&obj_prev_connections, "inports", &obj_prev_ports);

			if (result == SUCCESS) {
				result = get_value_from_map(&obj_prev_ports, (*port)->port_id, &obj_prev_port);
			}

			if (result == SUCCESS) {
				result = get_value_from_array(&obj_prev_port, 0, &obj_peer);
			}

			if (result == SUCCESS) {
				result = decode_string_from_array(&obj_peer, 0, &(*port)->peer_id);
			}

			if (strcmp((*port)->peer_id, get_node()->node_id) == 0) {
				(*port)->is_local = true;
			} else {
				(*port)->is_local = false;
			}

			if (result == SUCCESS) {
				result = decode_string_from_array(&obj_peer, 1, &(*port)->peer_port_id);
			}

			if (result == SUCCESS) {
				result = create_fifo(&(*port)->fifo, obj_queue);
				if (result != SUCCESS) {
					log_error("Failed to create fifo");
				}
			}
		} else {
			result = get_value_from_map(&obj_prev_connections, "outports", &obj_prev_ports);

			if (result == SUCCESS) {
				result = get_value_from_map(&obj_prev_ports, (*port)->port_id, &obj_prev_port);
			}

			if (result == SUCCESS) {
				result = get_value_from_array(&obj_prev_port, 0, &obj_peer);
			}

			if (result == SUCCESS) {
				result = decode_string_from_array(&obj_peer, 0, &(*port)->peer_id);
			}

			if (strcmp((*port)->peer_id, get_node()->node_id) == 0) {
				(*port)->is_local = true;
			} else {
				(*port)->is_local = false;
			}

			if (result == SUCCESS) {
				result = decode_string_from_array(&obj_peer, 1, &(*port)->peer_port_id);
			}

			if (result == SUCCESS) {
				result = create_fifo(&(*port)->fifo, obj_queue);
				if (result != SUCCESS) {
					log_error("Failed to create fifo");
				}
			}
		}
	}

	if (result == SUCCESS) {
		log_debug("Port '%s' created", (*port)->port_id);
		add_port(head, (*port));
	} else {
		free_port((*port), false);
	}

	return result;
}

void free_port(port_t *port, bool remove_from_storage)
{
	char *msg_uuid = NULL;
	node_t *node = get_node();

	if (port != NULL) {
		log_debug("Freeing port '%s'", port->port_id);
		if (remove_from_storage && node != NULL) {
			msg_uuid = gen_uuid("MSGID_");
			send_remove_port(msg_uuid, node->storage_tunnel, node->node_id, port);
			add_pending_msg(STORAGE_SET, msg_uuid);
		}
		free(port->port_id);
		free(port->port_name);
		free(port->peer_id);
		free(port->peer_port_id);
		free_fifo(port->fifo);
		if (port->tunnel != NULL) {
			port->tunnel->ref_count--;
		}
		free(port);
	}
}

result_t store_port(actor_t *actor, port_t *port)
{
	node_t *node = get_node();
	char *msg_uuid = gen_uuid("MSGID_");
	result_t result = send_set_port(msg_uuid, node->storage_tunnel, node->node_id, actor, port);

	if (result == SUCCESS) {
		result = add_pending_msg(STORAGE_SET, msg_uuid);
	} else {
		log_error("Failed to store port '%s'", port->port_id);
		free(msg_uuid);
	}

	return result;
}

result_t add_ports(actor_t *actor, port_t *ports)
{
	result_t result = SUCCESS;
	tunnel_t *tunnel = NULL;

	port_t *port = ports;
	while (port != NULL && result == SUCCESS) {
		if (!port->is_local) {
			tunnel = get_token_tunnel(port->peer_id);
			if (tunnel != NULL) {
				port->tunnel = tunnel;
				tunnel->ref_count++;
				if (tunnel->state == TUNNEL_CONNECTED) {
					result = connect_port(port);
					if (result != SUCCESS) {
						log_error("Failed to connect port '%s'", port->port_id);
						break;
					}
				}
			} else {
				log_error("Failed get token tunnel");
				result = FAIL;
				break;
			}
		} else {
			port->state = PORT_CONNECTED;
		}

		if (result == SUCCESS) {
			result = store_port(actor, port);
		}

		port = port->next;
	}

	return result;
}

port_t *get_inport(char *port_id)
{
	int i_actor = 0;
	port_t *port = NULL;
	node_t *node = get_node();

	for (i_actor = 0; i_actor < MAX_ACTORS; i_actor++) {
		if (node->actors[i_actor] != NULL) {
			port = node->actors[i_actor]->inports;
			while (port != NULL) {
				if (strcmp(port->port_id, port_id) == 0) {
					return port;
				}
				port = port->next;
			}
		}
	}

	return NULL;
}

port_t *get_outport(char *port_id)
{
	int i_actor = 0;
	port_t *port = NULL;
	node_t *node = get_node();

	for (i_actor = 0; i_actor < MAX_ACTORS; i_actor++) {
		if (node->actors[i_actor] != NULL) {
			port = node->actors[i_actor]->outports;
			while (port != NULL) {
				if (strcmp(port->port_id, port_id) == 0) {
					return port;
				}
				port = port->next;
			}
		}
	}

	return NULL;
}

result_t connect_port(port_t *port)
{
	result_t result = SUCCESS;
	char *msg_uuid = NULL;
	node_t *node = get_node();

	msg_uuid = gen_uuid("MSGID_");
	result = send_port_connect(msg_uuid, node->node_id, port);
	if (result == SUCCESS) {
		result = add_pending_msg(PORT_CONNECT, msg_uuid);
	} else {
		free(msg_uuid);
	}

	return result;
}

result_t disconnect_port(port_t *port)
{
	result_t result = SUCCESS;
	char *msg_uuid = NULL;
	node_t *node = get_node();

	msg_uuid = gen_uuid("MSGID_");
	result = send_port_disconnect(msg_uuid, node->node_id, port);
	if (result == SUCCESS) {
		result = add_pending_msg_with_data(PORT_DISCONNECT, msg_uuid, (void *)strdup(port->port_id));
	} else {
		free(msg_uuid);
	}

	return result;
}

result_t handle_port_disconnect(char *port_id)
{
	port_t *port = NULL;

	port = get_inport(port_id);
	if (port != NULL) {
		port->state = PORT_DISCONNECTED;
		return SUCCESS;
	}

	port = get_outport(port_id);
	if (port != NULL) {
		port->state = PORT_DISCONNECTED;
		return SUCCESS;
	}

	log_error("Disconnected port '%s'", port_id);

	return FAIL;
}

result_t port_connected(char *port_peer_id)
{
	int i_actor = 0;
	port_t *port = NULL;
	node_t *node = get_node();

	for (i_actor = 0; i_actor < MAX_ACTORS; i_actor++) {
		if (node->actors[i_actor] != NULL) {
			port = node->actors[i_actor]->inports;
			while (port != NULL) {
				if (strcmp(port->peer_port_id, port_peer_id) == 0) {
					log_debug("Port '%s' connected", port->port_id);
					port->state = PORT_CONNECTED;
					return SUCCESS;
				}
				port = port->next;
			}

			port = node->actors[i_actor]->outports;
			while (port != NULL) {
				if (strcmp(port->peer_port_id, port_peer_id) == 0) {
					log_debug("Port '%s' connected", port->port_id);
					port->state = PORT_CONNECTED;
					return SUCCESS;
				}
				port = port->next;
			}
		}
	}

	log_error("No port with peer port id '%s'", port_peer_id);

	return FAIL;
}

result_t handle_port_connect(char *port_id, char *peer_port_id)
{
	// TODO: Handle port_connect properly, currently always return FAIL as
	// connections are initiated by actors being migrated to this runtime
	return FAIL;
}