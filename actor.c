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
#include "node.h"
#include "actor.h"
#include "fifo.h"
#include "token.h"
#include "actors/actor_print.h"
#include "actors/actor_counttimer.h"
#include "actors/actor_identity.h"
#include "platform.h"
#include "msgpack_helper.h"
#include "msgpuck/msgpuck.h"
#include "proto.h"

#define NBR_OF_ACTOR_TYPES 3

struct actor_type_t {
	char type[50];
	result_t (*init_actor)(char *obj, actor_state_t **state);
	void (*free_state)(actor_t *actor);
	char* (*serialize_state)(actor_state_t *state, char **buffer);
	result_t (*fire_actor)(actor_t *actor);
};

struct actor_type_t actor_types[NBR_OF_ACTOR_TYPES] =
{
	{"std.CountTimer", actor_count_timer_init, free_count_timer_state, serialize_count_timer, actor_count_timer},
	{"io.Print", NULL, NULL, NULL, actor_print},
	{"std.Identity", NULL, NULL, NULL, actor_identity},
};

static result_t create_actor_from_type(char *type, actor_t **actor)
{
	int i = 0;

	for (i = 0; i < NBR_OF_ACTOR_TYPES; i++) {
		if (strcmp(type, actor_types[i].type) == 0) {
			*actor = (actor_t*)malloc(sizeof(actor_t));
			if (*actor == NULL) {
				log_error("Failed to allocate memory");
				return FAIL;
			}

			(*actor)->id = NULL;
			(*actor)->name = NULL;
			(*actor)->type = type;
			(*actor)->inports = NULL;
			(*actor)->outports = NULL;
			(*actor)->state = NULL;
			(*actor)->signature = NULL;
			(*actor)->managed_attr = NULL;
			(*actor)->enabled = false;
			(*actor)->init_actor = actor_types[i].init_actor;
			(*actor)->serialize_state = actor_types[i].serialize_state;
			(*actor)->free_state = actor_types[i].free_state;
			(*actor)->fire = actor_types[i].fire_actor;
			return SUCCESS;
		}
	}

	log_error("Actor type '%s' not supported", type);

	return FAIL;
}

result_t set_actor_reply_handler(char *data, void *msg_data)
{
	log_debug("TODO: set_actor_reply_handler does nothing");
	return SUCCESS;
}

result_t store_actor(const node_t *node, const actor_t *actor)
{
	result_t result = FAIL;

	result = send_set_actor(node, actor, set_actor_reply_handler);
	if (result != SUCCESS) {
		log_error("Failed to store actor %s", actor->name);
	}

	return result;
}

result_t add_actor(node_t *node, actor_t *actor)
{
	result_t result = FAIL;
	int i_actor = 0;

	for (i_actor = 0; i_actor < MAX_ACTORS; i_actor++) {
		if (node->actors[i_actor] == NULL) {
			node->actors[i_actor] = actor;
			result = SUCCESS;
			break;
		}
	}

	if (result == SUCCESS) {
		result = add_ports(node, actor, actor->inports);
	}

	if (result == SUCCESS) {
		result = add_ports(node, actor, actor->outports);
	}

	if (result == SUCCESS) {
		result = store_actor(node, actor);
	}

	return result;
}

result_t create_actor(node_t *node, char *root, actor_t **actor)
{
	result_t result = SUCCESS;
	port_t *port = NULL;
	char *type = NULL, *obj_state = NULL, *obj_actor_state = NULL, *obj_prev_connections = NULL;
	char *obj_ports = NULL, *obj_managed = NULL, *r = root;
	uint32_t i_port = 0, map_size = 0, i_managed_attr = 0, array_size = 0;
	managed_attributes_t *managed_attr = NULL, *tmp_attr = NULL;

	result = get_value_from_map(&r, "state", &obj_state);

	if (result == SUCCESS) {
		result = decode_string_from_map(&obj_state, "actor_type", &type);
	}

	if (result == SUCCESS) {
		result = create_actor_from_type(type, actor);
	}

	if (result == SUCCESS) {
		result = get_value_from_map(&obj_state, "actor_state", &obj_actor_state);
	}

	if (result == SUCCESS) {
		result = decode_string_from_map(&obj_actor_state, "id", &(*actor)->id);
	}

	if (result == SUCCESS) {
		result = decode_string_from_map(&obj_actor_state, "name", &(*actor)->name);
	}

	if (result == SUCCESS) {
		result = decode_string_from_map(&obj_actor_state, "_signature", &(*actor)->signature);
	}

	if (result == SUCCESS) {
		result = get_value_from_map(&obj_actor_state, "_managed", &obj_managed);
	}

	if (result == SUCCESS) {
		array_size = mp_decode_array((const char **)&obj_managed);
		for (i_managed_attr = 0; i_managed_attr < array_size; i_managed_attr++) {
			managed_attr = (managed_attributes_t*)malloc(sizeof(managed_attributes_t));
			if (managed_attr == NULL) {
				log_error("Failed to allocate memory");
				result = FAIL;
				break;
			}
			managed_attr->next = NULL;
			if (decode_str(&obj_managed, &managed_attr->attribute) != SUCCESS) {
				result = FAIL;
				break;
			}

			if ((*actor)->managed_attr == NULL) {
				(*actor)->managed_attr = managed_attr;
			} else {
				tmp_attr = (*actor)->managed_attr;
				while (tmp_attr->next != NULL) {
					tmp_attr = tmp_attr->next;
				}
				tmp_attr->next = managed_attr;
			}
		}
	}

	if (result == SUCCESS) {
		result = get_value_from_map(&obj_state, "prev_connections", &obj_prev_connections);
		if (result == SUCCESS) {
			result = get_value_from_map(&obj_actor_state, "inports", &obj_ports);
			if (result == SUCCESS) {
				if (mp_typeof(*obj_ports) == MP_MAP) {
					map_size = mp_decode_map((const char **)&obj_ports);
					for (i_port = 0; i_port < map_size; i_port++) {
						mp_next((const char **)&obj_ports);
						result = create_port(node, *actor, &port, &(*actor)->inports, obj_ports, obj_prev_connections, IN);
						if (result != SUCCESS) {
							log_error("Failed to create port");
							break;
						}
					}
				} else {
					log_error("'inports' is not a map");
					result = FAIL;
				}
			}
		}

		if (result == SUCCESS) {
			result = get_value_from_map(&obj_actor_state, "outports", &obj_ports);
			if (result == SUCCESS) {
				if (mp_typeof(*obj_ports) == MP_MAP) {
					map_size = mp_decode_map((const char **)&obj_ports);
					for (i_port = 0; i_port < map_size; i_port++) {
						mp_next((const char **)&obj_ports);
						result = create_port(node, *actor, &port, &(*actor)->outports, obj_ports, obj_prev_connections, OUT);
						if (result != SUCCESS) {
							log_error("Failed to create port");
							break;
						}
					}
				} else {
					log_error("'outports' is not a map");
					result = FAIL;
				}
			}
		}
	}

	if (result == SUCCESS && (*actor)->init_actor != NULL) {
		result = (*actor)->init_actor(obj_actor_state, &(*actor)->state);
	}

	if (result == SUCCESS) {
		result = add_actor(node, *actor);
	}

	if (result == SUCCESS) {
		log_debug("Actor '%s' created", (*actor)->name);
	} else {
		if (type != NULL) {
			free(type);
		}
		free_actor(node, *actor, false);
	}

	return SUCCESS;
}

result_t disconnect_actor(node_t *node, actor_t *actor)
{
	result_t result = SUCCESS;
	port_t *port = NULL, *tmp_port = NULL;

	if (actor != NULL) {
		log_debug("Disconnecting actor '%s'", actor->name);

		actor->enabled = false;

		port = actor->inports;
		while (port != NULL) {
			tmp_port = port;
			port = port->next;
			if (disconnect_port(node, tmp_port) != SUCCESS) {
				result = FAIL;
			}
		}

		port = actor->outports;
		while (port != NULL) {
			tmp_port = port;
			port = port->next;
			if (disconnect_port(node, tmp_port) != SUCCESS) {
				result = FAIL;
			}
		}
	} else {
		result = FAIL;
	}

	return result;
}

result_t remove_actor_reply_handler(char *data, void *msg_data)
{
	log_debug("remove_actor_reply_handler");
	return SUCCESS;
}

void free_actor(node_t *node, actor_t *actor, bool remove_from_storage)
{
	port_t *port = NULL, *tmp_port = NULL;
	managed_attributes_t *managed_attr = NULL;

	if (actor != NULL) {
		log_debug("Freeing actor '%s'", actor->id);
		if (remove_from_storage && node != NULL) {
			if (send_remove_actor(node, actor, remove_actor_reply_handler) != SUCCESS) {
				log_error("Failed to remove actor '%s'", actor->name);
			}
		}

		free(actor->id);
		free(actor->type);
		free(actor->name);
		free(actor->signature);

		while (actor->managed_attr != NULL) {
			managed_attr = actor->managed_attr;
			actor->managed_attr = actor->managed_attr->next;
			free(managed_attr->attribute);
			free(managed_attr);
		}

		port = actor->inports;
		while (port != NULL) {
			tmp_port = port;
			port = port->next;
			free_port(node, tmp_port, remove_from_storage);
		}

		port = actor->outports;
		while (port != NULL) {
			tmp_port = port;
			port = port->next;
			free_port(node, tmp_port, remove_from_storage);
		}

		if (actor->state != NULL && actor->free_state != NULL) {
			actor->free_state(actor);
		}

		free(actor);
	}
}

static int get_number_of_ports(port_t *port)
{
	int i = 0;
	port_t *tmp_port = port;

	while (tmp_port != NULL) {
		i++;
		tmp_port = tmp_port->next;
	}

	return i;
}

static int get_number_of_managed_attributes(managed_attributes_t *managed_attr)
{
	int i = 0;
	managed_attributes_t* tmp_attr = managed_attr;

	while (tmp_attr != NULL) {
		i++;
		tmp_attr = tmp_attr->next;
	}

	return i;
}

char *serialize_actor(const actor_t *actor, char **buffer)
{
	port_t *tmp_port = NULL;
	int nbr_inports = 0, nbr_outports = 0, i_token = 0;
	managed_attributes_t *managed_attr = NULL;
	int nbr_state_attributes = 9;

	if (actor == NULL) {
		log_error("NULL actor");
		return NULL;
	}

	nbr_inports = get_number_of_ports(actor->inports);
	nbr_outports = get_number_of_ports(actor->outports);

	// add number of actor state variables
	if (actor->state != NULL) {
		nbr_state_attributes += actor->state->nbr_attributes;
	}

	*buffer = encode_map(buffer, "state", 3);
	{
		*buffer = encode_str(buffer, "actor_type", actor->type);
		*buffer = encode_map(buffer, "prev_connections", 2);
		{
			*buffer = encode_map(buffer, "inports", nbr_inports);
			tmp_port = actor->inports;
			while (tmp_port != NULL) {
				*buffer = mp_encode_str(*buffer, tmp_port->port_id, strlen(tmp_port->port_id));
				*buffer = mp_encode_array(*buffer, 1);
				*buffer = mp_encode_array(*buffer, 2);
				*buffer = mp_encode_str(*buffer, tmp_port->peer_id, strlen(tmp_port->peer_id));
				*buffer = mp_encode_str(*buffer, tmp_port->peer_port_id, strlen(tmp_port->peer_port_id));
				tmp_port = tmp_port->next;
			}

 			*buffer = encode_map(buffer, "outports", nbr_outports);
 			tmp_port = actor->outports;
		 	while (tmp_port != NULL) {
		 		*buffer = mp_encode_str(*buffer, tmp_port->port_id, strlen(tmp_port->port_id));
		 		*buffer = mp_encode_array(*buffer, 1);
		 		*buffer = mp_encode_array(*buffer, 2);
		 		*buffer = mp_encode_str(*buffer, tmp_port->peer_id, strlen(tmp_port->peer_id));
		 		*buffer = mp_encode_str(*buffer, tmp_port->peer_port_id, strlen(tmp_port->peer_port_id));
		 		tmp_port = tmp_port->next;
		 	}
		 }


		*buffer = encode_map(buffer, "actor_state", nbr_state_attributes);
		{
			*buffer = encode_str(buffer, "id", actor->id);
			*buffer = encode_str(buffer, "name", actor->name);
			*buffer = encode_nil(buffer, "credentials");
			*buffer = encode_str(buffer, "_signature", actor->signature);
			*buffer = encode_array(buffer, "_deployment_requirements", 0);
			*buffer = encode_array(buffer, "_component_members", 1);
			{
				*buffer = mp_encode_str(*buffer, actor->id, strlen(actor->id));
			}
			*buffer = encode_array(buffer, "_managed", get_number_of_managed_attributes(actor->managed_attr));
			{
				managed_attr = actor->managed_attr;
				while (managed_attr != NULL) {
					*buffer = mp_encode_str(*buffer, managed_attr->attribute, strlen(managed_attr->attribute));
					managed_attr = managed_attr->next;
				}
			}

			if (actor->state != NULL && actor->serialize_state != NULL) {
				*buffer = actor->serialize_state(actor->state, buffer);
			}

			*buffer = encode_map(buffer, "inports", nbr_inports);
			{
				tmp_port = actor->inports;
				while (tmp_port != NULL) {
					*buffer = encode_map(buffer, tmp_port->port_name, 4);
					{
						*buffer = encode_str(buffer, "id", tmp_port->port_id);
						*buffer = encode_str(buffer, "name", tmp_port->port_name);
						*buffer = encode_map(buffer, "queue", 7);
						{
							*buffer = encode_str(buffer, "queuetype", "fanout_fifo");
							*buffer = encode_uint(buffer, "write_pos", tmp_port->fifo->write_pos);
							*buffer = encode_array(buffer, "readers", 1);
							{
								*buffer = mp_encode_str(*buffer, tmp_port->port_id, strlen(tmp_port->port_id));
							}
							*buffer = encode_uint(buffer, "N", tmp_port->fifo->size);
							*buffer = encode_map(buffer, "tentative_read_pos", 1);
							{
								*buffer = encode_uint(buffer, tmp_port->port_id, tmp_port->fifo->tentative_read_pos);
							}
							*buffer = encode_map(buffer, "read_pos", 1);
							{
								*buffer = encode_uint(buffer, tmp_port->port_id, tmp_port->fifo->read_pos);
							}
							*buffer = encode_array(buffer, "fifo", tmp_port->fifo->size);
							{
								for (i_token = 0; i_token < tmp_port->fifo->size; i_token++) {
									*buffer = encode_token(buffer, tmp_port->fifo->tokens[i_token], false);
								}
							}
						}
						*buffer = encode_map(buffer, "properties", 3);
						{
							*buffer = encode_uint(buffer, "nbr_peers", 1);
							*buffer = encode_str(buffer, "direction", "in");
							*buffer = encode_str(buffer, "routing", "default");
						}
					}
					tmp_port = tmp_port->next;
				}
			}

			*buffer = encode_map(buffer, "outports", nbr_outports);
			{
				tmp_port = actor->outports;
				while (tmp_port != NULL) {
					*buffer = encode_map(buffer, tmp_port->port_name, 4);
					{
						*buffer = encode_str(buffer, "id", tmp_port->port_id);
						*buffer = encode_str(buffer, "name", tmp_port->port_name);
						*buffer = encode_map(buffer, "queue", 7);
						{
							*buffer = encode_str(buffer, "queuetype", "fanout_fifo");
							*buffer = encode_uint(buffer, "write_pos", tmp_port->fifo->write_pos);
							*buffer = encode_array(buffer, "readers", 1);
							{
								*buffer = mp_encode_str(*buffer, tmp_port->peer_port_id, strlen(tmp_port->peer_port_id));
							}
							*buffer = encode_uint(buffer, "N", tmp_port->fifo->size);
							*buffer = encode_map(buffer, "tentative_read_pos", 1);
							{
								*buffer = encode_uint(buffer, tmp_port->peer_port_id, tmp_port->fifo->tentative_read_pos);
							}
							*buffer = encode_map(buffer, "read_pos", 1);
							{
								*buffer = encode_uint(buffer, tmp_port->peer_port_id, tmp_port->fifo->read_pos);
							}
							*buffer = encode_array(buffer, "fifo", tmp_port->fifo->size);
							{
								for (i_token = 0; i_token < tmp_port->fifo->size; i_token++) {
									*buffer = encode_token(buffer, tmp_port->fifo->tokens[i_token], false);
								}
							}
						}
						*buffer = encode_map(buffer, "properties", 3);
						{
							*buffer = encode_uint(buffer, "nbr_peers", 1);
							*buffer = encode_str(buffer, "direction", "out");
							*buffer = encode_str(buffer, "routing", "default");
						}
					}
					tmp_port = tmp_port->next;
				}
			}
		}
	}

	return *buffer;
}

void delete_actor(node_t *node, actor_t *actor, bool remove_from_storage)
{
	int i_actor = 0;

	for (i_actor = 0; i_actor < MAX_ACTORS; i_actor++) {
		if (node->actors[i_actor] != NULL && strcmp(node->actors[i_actor]->id, actor->id) == 0) {
			free_actor(node, actor, remove_from_storage);
			node->actors[i_actor] = NULL;
			return;
		}
	}
}

actor_t *get_actor(node_t *node, const char *actor_id)
{
	int i_actor = 0;

	for (i_actor = 0; i_actor < MAX_ACTORS; i_actor++) {
		if (node->actors[i_actor] != NULL && strcmp(node->actors[i_actor]->id, actor_id) == 0) {
			return node->actors[i_actor];
		}
	}

	return NULL;
}

result_t connect_actor(node_t *node, actor_t *actor, tunnel_t *tunnel)
{
	result_t result = SUCCESS;
	port_t *port = NULL;

	port = actor->inports;
	while (port != NULL && result == SUCCESS) {
		if (strcmp(tunnel->peer_id, port->peer_id) == 0) {
			result = connect_port(node, port, tunnel);
			if (result != SUCCESS) {
				log_error("Failed to connect port '%s'", port->port_id);
			}
		}
		port = port->next;
	}

	port = actor->outports;
	while (port != NULL && result == SUCCESS) {
		if (strcmp(tunnel->peer_id, port->peer_id) == 0) {
			result = connect_port(node, port, tunnel);
			if (result != SUCCESS) {
				log_error("Failed to connect port '%s'", port->port_id);
			}
		}
		port = port->next;
	}

	return result;
}

void enable_actor(actor_t *actor)
{
	port_t *port = NULL;
	bool connected = true;

	port = actor->inports;
	while (port != NULL && connected) {
		connected = port->state == PORT_CONNECTED ? true : false;
		port = port->next;
	}

	port = actor->outports;
	while (port != NULL && connected) {
		connected = port->state == PORT_CONNECTED ? true : false;
		port = port->next;
	}

	if (connected) {
		log_debug("Actor '%s' enabled", actor->name);
	}
	actor->enabled = connected;
}

void disable_actor(actor_t *actor)
{
	log_debug("Actor '%s' disabled", actor->name);
	actor->enabled = false;
}