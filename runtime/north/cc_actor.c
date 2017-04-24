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
#include <unistd.h>
#include "cc_node.h"
#include "cc_actor.h"
#include "cc_fifo.h"
#include "cc_token.h"
#include "../../actors/cc_actor_identity.h"
#include "../../actors/cc_actor_gpioreader.h"
#include "../../actors/cc_actor_gpiowriter.h"
#include "../../actors/cc_actor_temperature.h"
#include "../../actors/cc_actor_button.h"
#include "../south/platform/cc_platform.h"
#include "cc_msgpack_helper.h"
#include "../../msgpuck/msgpuck.h"
#include "cc_proto.h"
#ifdef MICROPYTHON
#include "../../actors/cc_actor_mpy.h"
#endif

#ifndef MICROPYTHON
#define NBR_OF_ACTOR_TYPES 5

struct actor_type_t {
	char type[50];
	result_t (*init)(actor_t **actor, list_t *attributes);
	result_t (*set_state)(actor_t **actor, list_t *attributes);
	void (*free_state)(actor_t *actor);
	bool (*fire_actor)(actor_t *actor);
	result_t (*get_managed_attributes)(actor_t *actor, list_t **attributes);
	void (*will_migrate)(actor_t *actor);
	void (*will_end)(actor_t *actor);
	void (*did_migrate)(actor_t *actor);
};

const struct actor_type_t actor_types[NBR_OF_ACTOR_TYPES] = {
	{
		"std.Identity",
		actor_identity_init,
		actor_identity_set_state,
		actor_identity_free,
		actor_identity_fire,
		actor_identity_get_managed_attributes,
		NULL,
		NULL,
		NULL
	},
	{
		"io.GPIOReader",
		actor_gpioreader_init,
		actor_gpioreader_set_state,
		actor_gpioreader_free,
		actor_gpioreader_fire,
		actor_gpioreader_get_managed_attributes,
		NULL,
		NULL,
		NULL
	},
	{
		"io.GPIOWriter",
		actor_gpiowriter_init,
		actor_gpiowriter_set_state,
		actor_gpiowriter_free,
		actor_gpiowriter_fire,
		actor_gpiowriter_get_managed_attributes,
		NULL,
		NULL,
		NULL
	},
	{
		"sensor.Temperature",
		actor_temperature_init,
		actor_temperature_set_state,
		actor_temperature_free,
		actor_temperature_fire,
		NULL,
		NULL,
		NULL,
		NULL
	},
	{
		"io.Button",
		actor_button_init,
		actor_button_set_state,
		actor_button_free,
		actor_button_fire,
		NULL,
		NULL,
		actor_button_will_end,
		NULL,
	}
};
#endif

static result_t actor_remove_reply_handler(node_t *node, char *data, void *msg_data)
{
	return SUCCESS;
}

static result_t actor_migrate_reply_handler(node_t *node, char *data, void *msg_data)
{
	uint32_t status = 0;
	char *value = NULL;
	actor_t *actor = NULL;

	actor = actor_get(node, (char *)msg_data, strlen((char *)msg_data));
	if (actor == NULL) {
		log_error("No actor with id '%s'", (char *)msg_data);
		return FAIL;
	}

	if (get_value_from_map(data, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(value, "status", &status) == SUCCESS) {
			if (status == 200) {
				log("Actor '%s' migrated", (char *)msg_data);
				if (actor->did_migrate != NULL)
					actor->did_migrate(actor);
				actor_free(node, actor, false);
			} else
				log_error("Failed to migrate actor '%s'", (char *)msg_data);
			return SUCCESS;
		}
	}

	log_error("Failed to decode message");
	return FAIL;
}

static result_t actor_set_reply_handler(node_t *node, char *data, void *msg_data)
{
	char *value = NULL;
	bool status = false;

	if (get_value_from_map(data, "value", &value) == SUCCESS) {
		if (decode_bool_from_map(value, "value", &status) == SUCCESS) {
			if (status != true)
				log("Failed to store actor '%s'", (char *)msg_data);
			return SUCCESS;
		}
	}

	log_error("Failed to decode data");
	return FAIL;
}

void actor_set_state(actor_t *actor, actor_state_t state)
{
	log_debug("Actor '%s' state '%d' -> '%d'", actor->id, actor->state, state);
	actor->state = state;
}

result_t actor_init_from_type(actor_t *actor, char *type, uint32_t type_len)
{
	strncpy(actor->type, type, type_len);
	actor->state = ACTOR_PENDING;
	actor->in_ports = NULL;
	actor->out_ports = NULL;
	actor->instance_state = NULL;
	actor->attributes = NULL;
	actor->will_migrate = NULL;
	actor->will_end = NULL;
	actor->did_migrate = NULL;

#ifdef MICROPYTHON
	if (actor_mpy_init_from_type(actor, actor->type, type_len) == SUCCESS)
		return SUCCESS;
#else
	int i = 0;

	for (i = 0; i < NBR_OF_ACTOR_TYPES; i++) {
		if (strncmp(type, actor_types[i].type, type_len) == 0) {
			actor->init = actor_types[i].init;
			actor->set_state = actor_types[i].set_state;
			actor->free_state = actor_types[i].free_state;
			actor->fire = actor_types[i].fire_actor;
			actor->get_managed_attributes = actor_types[i].get_managed_attributes;
			return SUCCESS;
		}
	}
#endif

	log_error("Actor type '%.*s' not supported", (int)type_len, type);

	return FAIL;
}

static void actor_free_managed(list_t *managed_attributes)
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

static result_t actor_get_managed(char *obj_attributes, char *obj_actor_state, list_t **attributes, list_t **instance_attributes)
{
	result_t result = SUCCESS;
	char *obj_attribute_value = NULL, *attribute_name = NULL, *attribute_value = NULL, *tmp_string = NULL;
	uint32_t len = 0, array_size = 0, i_managed_attr = 0;

	array_size = get_size_of_array(obj_attributes);
	for (i_managed_attr = 0; i_managed_attr < array_size && result == SUCCESS; i_managed_attr++) {
		result = decode_string_from_array(obj_attributes, i_managed_attr, &tmp_string, &len);
		if (result == SUCCESS) {
			if (strncmp(tmp_string, "_id", 3) == 0 || strncmp(tmp_string, "_name", 5) == 0 || strncmp(tmp_string, "_shadow_args", 12) == 0)
				continue;

			attribute_name = NULL;
			attribute_value = NULL;

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

			if (attribute_name[0] == '_')
				result = list_add(attributes, attribute_name, attribute_value, len);
			else
				result = list_add(instance_attributes, attribute_name, attribute_value, len);
		}
	}

	if (result != SUCCESS) {
		actor_free_managed(*instance_attributes);
		*instance_attributes = NULL;
	}

	return result;
}

static result_t actor_get_shadow_args(char *obj_shadow_args, list_t **instance_attributes)
{
	char *attributes = obj_shadow_args, *attribute_name = NULL, *attribute_value = NULL, *tmp = NULL;
	uint32_t i_attribute = 0, nbr_attributes = 0, len = 0;

	nbr_attributes = mp_decode_map((const char **)&attributes);
	for (i_attribute = 0; i_attribute < nbr_attributes; i_attribute++) {
		attribute_name = NULL;
		attribute_value = NULL;

		if (decode_str(attributes, &tmp, &len) != SUCCESS)
			return FAIL;

		if (platform_mem_alloc((void **)&attribute_name, len + 1) != SUCCESS) {
			log_error("Failed to allocate memory");
			return FAIL;
		}

		strncpy(attribute_name, tmp, len);
		attribute_name[len] = '\0';

		mp_next((const char **)&attributes);

		len = get_size_of_value(attributes);
		if (platform_mem_alloc((void **)&attribute_value, len) != SUCCESS) {
			log_error("Failed to allocate memory");
			platform_mem_free((void *)attribute_name);
			return FAIL;
		}
		memcpy(attribute_value, attributes, len);

		if (list_add(instance_attributes, attribute_name, attribute_value, len) != SUCCESS) {
			platform_mem_free((void *)attribute_name);
			platform_mem_free((void *)attribute_value);
			return FAIL;
		}

		mp_next((const char **)&attributes);
	}

	return SUCCESS;
}

static result_t actor_create_ports(node_t *node, actor_t *actor, char *obj_ports, char *obj_prev_connections, port_direction_t direction)
{
	result_t result = SUCCESS;
	char *ports = obj_ports, *prev_connections = obj_prev_connections;
	uint32_t nbr_ports = 0, i_port = 0, nbr_keys = 0, i_key = 0;
	port_t *port = NULL;

	nbr_ports = mp_decode_map((const char **)&ports);
	for (i_port = 0; i_port < nbr_ports && result == SUCCESS; i_port++) {
		mp_next((const char **)&ports);

		port = port_create(node, actor, ports, prev_connections, direction);
		if (port == NULL)
			return FAIL;

		if (direction == PORT_DIRECTION_IN)
			result = list_add(&actor->in_ports, port->id, (void *)port, sizeof(port_t));
		else
			result = list_add(&actor->out_ports, port->id, (void *)port, sizeof(port_t));

		nbr_keys = mp_decode_map((const char **)&ports);
		for (i_key = 0; i_key < nbr_keys; i_key++) {
			mp_next((const char **)&ports);
			mp_next((const char **)&ports);
		}
	}

	return result;
}

actor_t *actor_create(node_t *node, char *root)
{
	actor_t *actor = NULL;
	char *type = NULL, *obj_state = NULL, *obj_actor_state = NULL, *obj_prev_connections = NULL;
	char *obj_ports = NULL, *obj_managed = NULL, *obj_shadow_args = NULL, *r = root, *id = NULL, *name = NULL;
	uint32_t type_len = 0, id_len = 0, name_len = 0;
	list_t *instance_attributes = NULL;

	if (platform_mem_alloc((void **)&actor, sizeof(actor_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	memset(actor, 0, sizeof(actor_t));

	actor->calvinsys = node->calvinsys;

	if (get_value_from_map(r, "state", &obj_state) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (decode_string_from_map(obj_state, "actor_type", &type, &type_len) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (actor_init_from_type(actor, type, type_len) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (list_add(&node->actors, actor->id, (void *)actor, sizeof(actor_t)) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (get_value_from_map(obj_state, "actor_state", &obj_actor_state) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (decode_string_from_map(obj_actor_state, "_id", &id, &id_len) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}
	strncpy(actor->id, id, id_len);

	if (decode_string_from_map(obj_actor_state, "_name", &name, &name_len) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}
	strncpy(actor->name, name, name_len);

	if (get_value_from_map(obj_state, "prev_connections", &obj_prev_connections) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (get_value_from_map(obj_actor_state, "inports", &obj_ports) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (actor_create_ports(node, actor, obj_ports, obj_prev_connections, PORT_DIRECTION_IN) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (get_value_from_map(obj_actor_state, "outports", &obj_ports) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (actor_create_ports(node, actor, obj_ports, obj_prev_connections, PORT_DIRECTION_OUT) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (get_value_from_map(obj_actor_state, "_managed", &obj_managed) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (actor_get_managed(obj_managed, obj_actor_state, &actor->attributes, &instance_attributes) != SUCCESS) {
		actor_free(node, actor, false);
		return NULL;
	}

	if (has_key(obj_actor_state, "_shadow_args")) {
		if (actor->init != NULL) {
			if (get_value_from_map(obj_actor_state, "_shadow_args", &obj_shadow_args) != SUCCESS) {
				actor_free(node, actor, false);
				return NULL;
			}

			if (actor_get_shadow_args(obj_shadow_args, &instance_attributes) != SUCCESS) {
				actor_free(node, actor, false);
				return NULL;
			}

			if (actor->init(&actor, instance_attributes) != SUCCESS) {
				actor_free_managed(instance_attributes);
				actor_free(node, actor, false);
				return NULL;
			}
		}
	} else {
		if (actor->set_state != NULL && actor->set_state(&actor, instance_attributes) != SUCCESS) {
			actor_free_managed(instance_attributes);
			actor_free(node, actor, false);
			return NULL;
		}
	}

	if (node->transport_client != NULL) {
		if (proto_send_set_actor(node, actor, actor_set_reply_handler) != SUCCESS) {
			actor_free_managed(instance_attributes);
			actor_free(node, actor, false);
			return NULL;
		}
	}

	// attributes should now be handled by the instance
	actor_free_managed(instance_attributes);

	log("Actor '%s' created with id '%s' and type '%s'", actor->name, actor->id, actor->type);

	return actor;
}

void actor_free(node_t *node, actor_t *actor, bool remove_from_registry)
{
	list_t *list = NULL, *tmp_list = NULL;

	log("Deleting actor '%s'", actor->name);

	if (actor->will_end != NULL)
		actor->will_end(actor);

	if (remove_from_registry) {
		if (proto_send_remove_actor(node, actor, actor_remove_reply_handler) != SUCCESS)
			log_error("Failed to remove actor '%s'", actor->id);
	}

	if (actor->instance_state != NULL && actor->free_state != NULL)
		actor->free_state(actor);

	list = actor->in_ports;
	while (list != NULL) {
		tmp_list = list;
		list = list->next;
		port_free(node, (port_t *)tmp_list->data);
		platform_mem_free((void *)tmp_list);
	}

	list = actor->out_ports;
	while (list != NULL) {
		tmp_list = list;
		list = list->next;
		port_free(node, (port_t *)tmp_list->data);
		platform_mem_free((void *)tmp_list);
	}

	list_remove(&node->actors, actor->id);
	actor_free_managed(actor->attributes);
	platform_mem_free((void *)actor);
	actor = NULL;
}

actor_t *actor_get(node_t *node, const char *actor_id, uint32_t actor_id_len)
{
	return (actor_t *)list_get(node->actors, actor_id);
}

void actor_port_enabled(actor_t *actor)
{
	list_t *list = NULL;

	list = actor->in_ports;
	while (list != NULL) {
		if (((port_t *)list->data)->state != PORT_ENABLED)
			return;
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		if (((port_t *)list->data)->state != PORT_ENABLED)
			return;
		list = list->next;
	}

	actor_set_state(actor, ACTOR_ENABLED);
}

void actor_port_disconnected(actor_t *actor)
{
	actor_set_state(actor, ACTOR_PENDING);
}

void actor_disconnect(node_t *node, actor_t *actor)
{
	list_t *list = NULL;

	list = actor->in_ports;
	while (list != NULL) {
		port_disconnect(node, (port_t *)list->data);
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		port_disconnect(node, (port_t *)list->data);
		list = list->next;
	}

	actor_set_state(actor, ACTOR_PENDING);
}

result_t actor_migrate(node_t *node, actor_t *actor, char *to_rt_uuid, uint32_t to_rt_uuid_len)
{
	list_t *list = NULL;

	if (actor->will_migrate != NULL)
		actor->will_migrate(actor);

	list = actor->in_ports;
	while (list != NULL) {
		port_disconnect(node, (port_t *)list->data);
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		port_disconnect(node, (port_t *)list->data);
		list = list->next;
	}

	if (proto_send_actor_new(node, actor, to_rt_uuid, to_rt_uuid_len, actor_migrate_reply_handler) == SUCCESS)
		return SUCCESS;

	log_error("Failed to migrate actor '%s'", actor->id);
	return FAIL;
}

char *actor_serialize(const node_t *node, const actor_t *actor, char **buffer, bool include_state)
{
	list_t *in_ports = NULL, *out_ports = NULL;
	list_t *instance_attributes = NULL, *tmp_list = NULL;
	port_t *port = NULL;
	unsigned int nbr_state_attributes = 3, nbr_port_attributes = 4;
	unsigned int nbr_inports = 0, nbr_outports = 0, i_token = 0, nbr_managed_attributes = 0;
	char *peer_id = NULL;

	if (actor->get_managed_attributes != NULL) {
		if (actor->get_managed_attributes((actor_t *)actor, &instance_attributes) != SUCCESS)
			log_error("Failed to get instance attributes");
	}

	nbr_managed_attributes = 2; // _id/_name
	nbr_managed_attributes += list_count(actor->attributes);
	if (instance_attributes != NULL)
		nbr_managed_attributes += list_count(instance_attributes);
	in_ports = actor->in_ports;
	nbr_inports = list_count(in_ports);
	out_ports = actor->out_ports;
	nbr_outports = list_count(out_ports);

	if (include_state) {
		nbr_state_attributes += 1;
		nbr_port_attributes += 1;
	}

	*buffer = encode_map(buffer, "state", nbr_state_attributes);
	{
		if (include_state)
			*buffer = encode_uint(buffer, "constrained_state", actor->state);
		*buffer = encode_str(buffer, "actor_type", actor->type, strlen(actor->type));
		*buffer = encode_map(buffer, "prev_connections", 2);
		{
			*buffer = encode_map(buffer, "inports", nbr_inports);
			in_ports = actor->in_ports;
			while (in_ports != NULL) {
				port = (port_t *)in_ports->data;
				peer_id = port_get_peer_id(node, port);
				*buffer = mp_encode_str(*buffer, port->id, strlen(port->id));
				*buffer = mp_encode_array(*buffer, 1);
				*buffer = mp_encode_array(*buffer, 2);
				if (peer_id != NULL)
					*buffer = mp_encode_str(*buffer, peer_id, strlen(peer_id));
				else
					*buffer = mp_encode_nil(*buffer);
				*buffer = mp_encode_str(*buffer, port->peer_port_id, strlen(port->peer_port_id));
				in_ports = in_ports->next;
			}

			*buffer = encode_map(buffer, "outports", nbr_outports);
			out_ports = actor->out_ports;
			while (out_ports != NULL) {
				port = (port_t *)out_ports->data;
				peer_id = port_get_peer_id(node, port);
				*buffer = mp_encode_str(*buffer, port->id, strlen(port->id));
				*buffer = mp_encode_array(*buffer, 1);
				*buffer = mp_encode_array(*buffer, 2);
				if (peer_id != NULL)
					*buffer = mp_encode_str(*buffer, peer_id, strlen(peer_id));
				else
					*buffer = mp_encode_nil(*buffer);
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
				*buffer = mp_encode_str(*buffer, "_id", 3);
				*buffer = mp_encode_str(*buffer, "_name", 5);

				tmp_list = actor->attributes;
				while (tmp_list != NULL) {
					*buffer = mp_encode_str(*buffer, tmp_list->id, strlen(tmp_list->id));
					tmp_list = tmp_list->next;
				}

				tmp_list = instance_attributes;
				while (tmp_list != NULL) {
					*buffer = mp_encode_str(*buffer, tmp_list->id, strlen(tmp_list->id));
					tmp_list = tmp_list->next;
				}
			}

			{
				*buffer = encode_str(buffer, "_id", actor->id, strlen(actor->id));
				*buffer = encode_str(buffer, "_name", actor->name, strlen(actor->name));

				tmp_list = actor->attributes;
				while (tmp_list != NULL) {
					*buffer = encode_value(buffer, tmp_list->id, (char *)tmp_list->data, tmp_list->data_len);
					tmp_list = tmp_list->next;
				}

				tmp_list = instance_attributes;
				while (tmp_list != NULL) {
					*buffer = encode_value(buffer, tmp_list->id, (char *)tmp_list->data, tmp_list->data_len);
					tmp_list = tmp_list->next;
				}
				actor_free_managed(instance_attributes);
			}

			*buffer = encode_map(buffer, "inports", nbr_inports);
			{
				in_ports = actor->in_ports;
				while (in_ports != NULL) {
					port = (port_t *)in_ports->data;
					*buffer = encode_map(buffer, port->name, nbr_port_attributes);
					{
						if (include_state)
							*buffer = encode_uint(buffer, "constrained_state", port->state);
						*buffer = encode_str(buffer, "id", port->id, strlen(port->id));
						*buffer = encode_str(buffer, "name", port->name, strlen(port->name));
						*buffer = encode_map(buffer, "queue", 7);
						{
							*buffer = encode_str(buffer, "queuetype", "fanout_fifo", strlen("fanout_fifo"));
							*buffer = encode_uint(buffer, "write_pos", port->fifo.write_pos);
							*buffer = encode_array(buffer, "readers", 1);
							{
								*buffer = mp_encode_str(*buffer, port->id, strlen(port->id));
							}
							*buffer = encode_uint(buffer, "N", port->fifo.size);
							*buffer = encode_map(buffer, "tentative_read_pos", 1);
							{
								*buffer = encode_uint(buffer, port->id, port->fifo.tentative_read_pos);
							}
							*buffer = encode_map(buffer, "read_pos", 1);
							{
								*buffer = encode_uint(buffer, port->id, port->fifo.read_pos);
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
					*buffer = encode_map(buffer, port->name, nbr_port_attributes);
					{
						if (include_state)
							*buffer = encode_uint(buffer, "constrained_state", port->state);
						*buffer = encode_str(buffer, "id", port->id, strlen(port->id));
						*buffer = encode_str(buffer, "name", port->name, strlen(port->name));
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
