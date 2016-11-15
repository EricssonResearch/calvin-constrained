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
#include "actors/actor_identity.h"
#include "actors/actor_gpioreader.h"
#include "actors/actor_gpiowriter.h"
#include "actors/actor_temperature.h"
#include "platform.h"
#include "msgpack_helper.h"
#include "msgpuck/msgpuck.h"
#include "proto.h"

#define NBR_OF_ACTOR_TYPES 4

struct actor_type_t {
	char type[50];
	result_t (*init)(actor_t **actor, char *obj_actor_state);
	result_t (*set_state)(actor_t **actor, char *obj_actor_state);
	void (*free_state)(actor_t *actor);
	result_t (*fire_actor)(actor_t *actor);
	char *(*serialize_state)(actor_t *actor, char **buffer);
	list_t *(*get_managed_attributes)(actor_t *actor);
};

const struct actor_type_t actor_types[NBR_OF_ACTOR_TYPES] = {
	{
		"std.Identity",
		actor_identity_init,
		actor_identity_set_state,
		actor_identity_free,
		actor_identity_fire,
		actor_identity_serialize,
		actor_identity_get_managed_attributes
	},
	{
		"io.GPIOReader",
		actor_gpioreader_init,
		actor_gpioreader_set_state,
		actor_gpioreader_free,
		actor_gpioreader_fire,
		actor_gpioreader_serialize,
		actor_gpioreader_get_managed_attributes
	},
	{
		"io.GPIOWriter",
		actor_gpiowriter_init,
		actor_gpiowriter_set_state,
		actor_gpiowriter_free,
		actor_gpiowriter_fire,
		actor_gpiowriter_serialize,
		actor_gpiowriter_get_managed_attributes
	},
	{
		"sensor.Temperature",
		actor_temperature_init,
		actor_temperature_set_state,
		actor_temperature_free,
		actor_temperature_fire,
		actor_temperature_serialize,
		actor_temperature_get_managed_attributes
	}
};

static result_t actor_remove_reply_handler(char *data, void *msg_data)
{
	node_t *node = node_get();
	actor_t *actor = NULL;

	actor = actor_get(node, (char *)msg_data, strlen((char *)msg_data));
	if (actor != NULL) {
		actor_free(node, actor);
		return SUCCESS;
	}

	log_error("No actor with id '%s'", (char *)msg_data);

	return FAIL;
}

static result_t actor_migrate_reply_handler(char *data, void *msg_data)
{
	node_t *node = node_get();
	actor_t *actor = NULL;
	uint32_t status = 0;
	char *value = NULL;

	if (get_value_from_map(data, "value", &value) != SUCCESS)
		return FAIL;

	if (decode_uint_from_map(value, "status", &status) != SUCCESS)
		return FAIL;

	actor = actor_get(node, (char *)msg_data, strlen((char *)msg_data));
	if (actor == NULL) {
		log_error("No actor with id '%s'", (char *)msg_data);
		return FAIL;
	}

	actor_free(node, actor);

	return SUCCESS;
}

static result_t actor_set_reply_handler(char *data, void *msg_data)
{
	// TODO: Check result
	return SUCCESS;
}

static result_t actor_init_from_type(actor_t *actor, char *type, uint32_t type_len)
{
	int i = 0;

	for (i = 0; i < NBR_OF_ACTOR_TYPES; i++) {
		if (strncmp(type, actor_types[i].type, type_len) == 0) {
			strncpy(actor->type, type, type_len);
			actor->state = ACTOR_PENDING;
			actor->in_ports = NULL;
			actor->out_ports = NULL;
			actor->instance_state = NULL;
			actor->init = actor_types[i].init;
			actor->set_state = actor_types[i].set_state;
			actor->free_state = actor_types[i].free_state;
			actor->fire = actor_types[i].fire_actor;
			actor->serialize_state = actor_types[i].serialize_state;
			actor->get_managed_attributes = actor_types[i].get_managed_attributes;
			return SUCCESS;
		}
	}

	log_error("Actor type '%.*s' not supported", (int)type_len, type);

	return FAIL;
}

void actor_free_managed(list_t *managed_attributes)
{
	list_t *tmp_list = NULL;

	while (managed_attributes != NULL) {
		tmp_list = managed_attributes;
		managed_attributes = managed_attributes->next;
		platform_mem_free((void *)tmp_list->id);
		platform_mem_free((void *)tmp_list->data);
		platform_mem_free((void *)tmp_list);
	}
}

result_t actor_get_managed(char *obj_actor_state, list_t **managed_attributes)
{
	result_t result = SUCCESS;
	char *obj_managed = NULL, *obj_attribute_value = NULL, *attribute_name = NULL, *tmp_string = NULL, *attribute_value = NULL;
	uint32_t len = 0, array_size = 0, i_managed_attr = 0;

	if (get_value_from_map(obj_actor_state, "_managed", &obj_managed) != SUCCESS)
		return FAIL;

	array_size = get_size_of_array(obj_managed);
	for (i_managed_attr = 0; i_managed_attr < array_size && result == SUCCESS; i_managed_attr++) {
		result = decode_string_from_array(obj_managed, i_managed_attr, &tmp_string, &len);
		if (result == SUCCESS) {
			attribute_name = NULL;
			attribute_value = NULL;
			if (strncmp(tmp_string, "_shadow_args", strlen("_shadow_args")) == 0)
				continue;

			if (platform_mem_alloc((void **)&attribute_name, len + 1) != SUCCESS) {
				log_error("Failed to allocate memory");
				return FAIL;
			}
			strncpy(attribute_name, tmp_string, len);
			attribute_name[len] = '\0';

			if (get_value_from_map(obj_actor_state, attribute_name, &obj_attribute_value) != SUCCESS) {
				log_error("Failed to decode '%s'", attribute_name);
				platform_mem_free((void *)attribute_name);
				return FAIL;
			}

			len = get_size_of_value(obj_attribute_value);
			if (platform_mem_alloc((void **)&attribute_value, len) != SUCCESS) {
				log_error("Failed to allocate memory");
				platform_mem_free((void *)attribute_name);
				return FAIL;
			}
			memcpy(attribute_value, obj_attribute_value, len);

			result = list_add(managed_attributes, attribute_name, attribute_value, len);
		}
	}

	if (result != SUCCESS) {
		actor_free_managed(*managed_attributes);
		*managed_attributes = NULL;
	}

	return result;
}

static result_t actor_create_ports(node_t *node, actor_t *actor, char *obj_ports, char *obj_prev_connections, port_direction_t direction)
{
	result_t result = SUCCESS;
	char *ports = obj_ports, *prev_connections = obj_prev_connections;
	uint32_t map_size = 0, i_port = 0;
	port_t *port = NULL;

	map_size = mp_decode_map((const char **)&ports);
	for (i_port = 0; i_port < map_size && result == SUCCESS; i_port++) {
		mp_next((const char **)&ports);
		if (direction == PORT_DIRECTION_IN)
			port = port_create(node, actor, ports, prev_connections, PORT_DIRECTION_IN);
		else
			port = port_create(node, actor, ports, prev_connections, PORT_DIRECTION_OUT);

		if (port == NULL)
			return FAIL;

		if (direction == PORT_DIRECTION_IN)
			result = list_add(&actor->in_ports, port->port_id, (void *)port, sizeof(port_t));
		else
			result = list_add(&actor->out_ports, port->port_id, (void *)port, sizeof(port_t));
	}

	return result;
}

actor_t *actor_create(node_t *node, char *root)
{
	actor_t *actor = NULL;
	char *type = NULL, *obj_state = NULL, *obj_actor_state = NULL, *obj_prev_connections = NULL;
	char *obj_ports = NULL, *r = root, *id = NULL, *name = NULL;
	uint32_t type_len = 0, id_len = 0, name_len = 0;

	if (platform_mem_alloc((void **)&actor, sizeof(actor_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}
	memset(actor, 0, sizeof(actor_t));

	if (get_value_from_map(r, "state", &obj_state) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}

	if (decode_string_from_map(obj_state, "actor_type", &type, &type_len) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}

	if (actor_init_from_type(actor, type, type_len) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}

	if (list_add(&node->actors, actor->id, (void *)actor, sizeof(actor_t)) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}

	if (get_value_from_map(obj_state, "actor_state", &obj_actor_state) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}

	if (decode_string_from_map(obj_actor_state, "id", &id, &id_len) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}
	strncpy(actor->id, id, id_len);

	if (decode_string_from_map(obj_actor_state, "name", &name, &name_len) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}
	strncpy(actor->name, name, name_len);

	if (get_value_from_map(obj_state, "prev_connections", &obj_prev_connections) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}

	if (get_value_from_map(obj_actor_state, "inports", &obj_ports) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}

	if (actor_create_ports(node, actor, obj_ports, obj_prev_connections, PORT_DIRECTION_IN) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}

	if (get_value_from_map(obj_actor_state, "outports", &obj_ports) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}

	if (actor_create_ports(node, actor, obj_ports, obj_prev_connections, PORT_DIRECTION_OUT) != SUCCESS) {
		actor_free(node, actor);
		return NULL;
	}

	if (has_key(obj_actor_state, "_shadow_args")) {
		if (actor->init(&actor, obj_actor_state) != SUCCESS) {
			actor_free(node, actor);
			return NULL;
		}
	} else {
		if (actor->set_state(&actor, obj_actor_state) != SUCCESS) {
			actor_free(node, actor);
			return NULL;
		}
	}

	return actor;
}

void actor_free(node_t *node, actor_t *actor)
{
	list_t *list = NULL, *tmp_list = NULL;

	log_debug("Freeing actor '%s'", actor->name);

	if (actor->instance_state != NULL && actor->free_state != NULL)
		actor->free_state(actor);

	list = actor->in_ports;
	while (list != NULL) {
		tmp_list = list;
		list = list->next;
		port_free((port_t *)tmp_list->data);
		platform_mem_free((void *)tmp_list);
	}

	list = actor->out_ports;
	while (list != NULL) {
		tmp_list = list;
		list = list->next;
		port_free((port_t *)tmp_list->data);
		platform_mem_free((void *)tmp_list);
	}

	list_remove(&node->actors, actor->id);
	platform_mem_free((void *)actor);
	actor = NULL;
}

actor_t *actor_get(node_t *node, const char *actor_id, uint32_t actor_id_len)
{
	list_t *list = NULL;

	list = list_get(node->actors, actor_id);
	if (list != NULL)
		return (actor_t *)list->data;

	return NULL;
}

void actor_port_enabled(actor_t *actor)
{
	port_t *port = NULL;
	list_t *list = actor->in_ports;

	while (list != NULL) {
		port = (port_t *)list->data;
		if (port->state != PORT_ENABLED)
			return;
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		port = (port_t *)list->data;
		if (port->state != PORT_ENABLED)
			return;
		list = list->next;
	}

	log_debug("Enabled '%s'", actor->name);

	actor->state = ACTOR_DO_ENABLE;
}

void actor_delete(actor_t *actor)
{
	list_t *list = actor->in_ports;

	actor->state = ACTOR_DO_DELETE;

	while (list != NULL) {
		port_delete((port_t *)list->data);
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		port_delete((port_t *)list->data);
		list = list->next;
	}
}

char *actor_serialize_managed_list(list_t *managed_attributes, char **buffer)
{
	list_t *list = managed_attributes;

	while (list != NULL) {
		*buffer = encode_value(buffer, list->id, list->data, list->data_len);
		list = list->next;
	}

	return *buffer;
}

result_t actor_migrate(actor_t *actor, char *to_rt_uuid, uint32_t to_rt_uuid_len)
{
	strncpy(actor->migrate_to, to_rt_uuid, to_rt_uuid_len);
	actor->state = ACTOR_DO_MIGRATE;
	return SUCCESS;
}

char *actor_serialize(const actor_t *actor, char **buffer)
{
	list_t *in_ports = NULL, *out_ports = NULL;
	list_t *managed_attributes = NULL;
	port_t *port = NULL;
	int nbr_inports = 0, nbr_outports = 0, i_token = 0, nbr_managed_attributes = 0;

	managed_attributes = actor->get_managed_attributes((actor_t *)actor);
	nbr_managed_attributes = list_count(managed_attributes);
	in_ports = actor->in_ports;
	nbr_inports = list_count(in_ports);
	out_ports = actor->out_ports;
	nbr_outports = list_count(out_ports);

	*buffer = encode_map(buffer, "state", 3);
	{
		*buffer = encode_str(buffer, "actor_type", actor->type, strlen(actor->type));
		*buffer = encode_map(buffer, "prev_connections", 2);
		{
			*buffer = encode_map(buffer, "inports", nbr_inports);
			in_ports = actor->in_ports;
			while (in_ports != NULL) {
				port = (port_t *)in_ports->data;
				*buffer = mp_encode_str(*buffer, port->port_id, strlen(port->port_id));
				*buffer = mp_encode_array(*buffer, 1);
				*buffer = mp_encode_array(*buffer, 2);
				*buffer = mp_encode_str(*buffer, port->peer_id, strlen(port->peer_id));
				*buffer = mp_encode_str(*buffer, port->peer_port_id, strlen(port->peer_port_id));
				in_ports = in_ports->next;
			}

			*buffer = encode_map(buffer, "outports", nbr_outports);
			out_ports = actor->out_ports;
			while (out_ports != NULL) {
				port = (port_t *)out_ports->data;
				*buffer = mp_encode_str(*buffer, port->port_id, strlen(port->port_id));
				*buffer = mp_encode_array(*buffer, 1);
				*buffer = mp_encode_array(*buffer, 2);
				*buffer = mp_encode_str(*buffer, port->peer_id, strlen(port->peer_id));
				*buffer = mp_encode_str(*buffer, port->peer_port_id, strlen(port->peer_port_id));
				out_ports = out_ports->next;
			}
		}

		*buffer = encode_map(buffer, "actor_state", nbr_managed_attributes + 4);
		{
			*buffer = encode_array(buffer, "_component_members", 1);
			{
				*buffer = mp_encode_str(*buffer, actor->id, strlen(actor->id));
			}
			*buffer = encode_array(buffer, "_managed", nbr_managed_attributes);
			{
				while (managed_attributes != NULL) {
					*buffer = mp_encode_str(*buffer, managed_attributes->id, strlen(managed_attributes->id));
					managed_attributes = managed_attributes->next;
				}
			}

			if (actor->serialize_state != NULL)
				*buffer = actor->serialize_state((actor_t *)actor, buffer);

			*buffer = encode_map(buffer, "inports", nbr_inports);
			{
				in_ports = actor->in_ports;
				while (in_ports != NULL) {
					port = (port_t *)in_ports->data;
					*buffer = encode_map(buffer, port->port_name, 4);
					{
						*buffer = encode_str(buffer, "id", port->port_id, strlen(port->port_id));
						*buffer = encode_str(buffer, "name", port->port_name, strlen(port->port_name));
						*buffer = encode_map(buffer, "queue", 7);
						{
							*buffer = encode_str(buffer, "queuetype", "fanout_fifo", strlen("fanout_fifo"));
							*buffer = encode_uint(buffer, "write_pos", port->fifo.write_pos);
							*buffer = encode_array(buffer, "readers", 1);
							{
								*buffer = mp_encode_str(*buffer, port->port_id, strlen(port->port_id));
							}
							*buffer = encode_uint(buffer, "N", port->fifo.size);
							*buffer = encode_map(buffer, "tentative_read_pos", 1);
							{
								*buffer = encode_uint(buffer, port->port_id, port->fifo.tentative_read_pos);
							}
							*buffer = encode_map(buffer, "read_pos", 1);
							{
								*buffer = encode_uint(buffer, port->port_id, port->fifo.read_pos);
							}
							*buffer = encode_array(buffer, "fifo", port->fifo.size);
							{
								for (i_token = 0; i_token < port->fifo.size; i_token++)
									*buffer = token_encode(buffer, port->fifo.tokens[i_token], false);
							}
						}
						*buffer = encode_map(buffer, "properties", 3);
						{
							*buffer = encode_uint(buffer, "nbr_peers", 1);
							*buffer = encode_str(buffer, "direction", "in", 2);
							*buffer = encode_str(buffer, "routing", "default", 7);
						}
					}
					in_ports = in_ports->next;
				}
			}

			*buffer = encode_map(buffer, "outports", nbr_outports);
			{
				out_ports = actor->out_ports;
				while (out_ports != NULL) {
					port = (port_t *)out_ports->data;
					*buffer = encode_map(buffer, port->port_name, 4);
					{
						*buffer = encode_str(buffer, "id", port->port_id, 2);
						*buffer = encode_str(buffer, "name", port->port_name, strlen(port->port_name));
						*buffer = encode_map(buffer, "queue", 7);
						{
							*buffer = encode_str(buffer, "queuetype", "fanout_fifo", 11);
							*buffer = encode_uint(buffer, "write_pos", port->fifo.write_pos);
							*buffer = encode_array(buffer, "readers", 1);
							{
								*buffer = mp_encode_str(*buffer, port->peer_port_id, strlen(port->peer_port_id));
							}
							*buffer = encode_uint(buffer, "N", port->fifo.size);
							*buffer = encode_map(buffer, "tentative_read_pos", 1);
							{
								*buffer = encode_uint(buffer, port->peer_port_id, port->fifo.tentative_read_pos);
							}
							*buffer = encode_map(buffer, "read_pos", 1);
							{
								*buffer = encode_uint(buffer, port->peer_port_id, port->fifo.read_pos);
							}
							*buffer = encode_array(buffer, "fifo", port->fifo.size);
							{
								for (i_token = 0; i_token < port->fifo.size; i_token++)
									*buffer = token_encode(buffer, port->fifo.tokens[i_token], false);
							}
						}
						*buffer = encode_map(buffer, "properties", 3);
						{
							*buffer = encode_uint(buffer, "nbr_peers", 1);
							*buffer = encode_str(buffer, "direction", "out", 3);
							*buffer = encode_str(buffer, "routing", "default", 7);
						}
					}
					out_ports = out_ports->next;
				}
			}
		}
	}

	return *buffer;
}

result_t actor_transmit(node_t *node, actor_t *actor)
{
	list_t *list = NULL;
	port_t *port = NULL;

	switch (actor->state) {
	case ACTOR_DO_ENABLE:
		actor->state = ACTOR_ENABLED;
		if (proto_send_set_actor(node, actor, actor_set_reply_handler) == SUCCESS)
			return SUCCESS;
		break;
	case ACTOR_DO_DELETE:
		actor->state = ACTOR_PENDING;
		if (proto_send_remove_actor(node, actor, actor_remove_reply_handler) == SUCCESS)
			return SUCCESS;
		actor->state = ACTOR_DO_DELETE;
		break;
	case ACTOR_DO_MIGRATE:
		list = actor->in_ports;
		while (list != NULL) {
			port = (port_t *)list->data;
			if (port->state == PORT_ENABLED) {
				port->state = PORT_DO_DISCONNECT;
				if (port_transmit(node, port) == SUCCESS)
					return SUCCESS;
			} else if (port->state == PORT_PENDING)
				return SUCCESS;
			list = list->next;
		}
		list = actor->out_ports;
		while (list != NULL) {
			port = (port_t *)list->data;
			if (port->state == PORT_ENABLED) {
				port->state = PORT_DO_DISCONNECT;
				if (port_transmit(node, port) == SUCCESS)
					return SUCCESS;
			} else if (port->state == PORT_PENDING)
				return SUCCESS;
			list = list->next;
		}
		actor->state = ACTOR_PENDING;
		if (proto_send_actor_new(node, actor, actor_migrate_reply_handler) == SUCCESS)
			return SUCCESS;
		actor->state = ACTOR_DO_MIGRATE;
	default:
		list = actor->in_ports;
		while (list != NULL) {
			port = (port_t *)list->data;
			if (port_transmit(node, port) == SUCCESS)
				return SUCCESS;
			list = list->next;
		}
		list = actor->out_ports;
		while (list != NULL) {
			port = (port_t *)list->data;
			if (port_transmit(node, port) == SUCCESS)
				return SUCCESS;
			list = list->next;
		}
	}

	return FAIL;
}
