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
#include "cc_actor_store.h"
#include "cc_fifo.h"
#include "cc_token.h"
#include "../south/platform/cc_platform.h"
#include "coder/cc_coder.h"
#include "cc_proto.h"
#ifdef CC_PYTHON_ENABLED
#include "../../actors/cc_actor_mpy.h"
#endif

static cc_result_t actor_remove_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	return CC_SUCCESS;
}

static cc_result_t cc_actor_migrate_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	uint32_t status = 0;
	char *value = NULL;
	cc_actor_t *actor = NULL;

	actor = cc_actor_get(node, (char *)msg_data, strnlen((char *)msg_data, CC_UUID_BUFFER_SIZE));
	if (actor == NULL) {
		cc_log_error("No actor with id '%s'", (char *)msg_data);
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(data, "value", &value) == CC_SUCCESS) {
		if (cc_coder_decode_uint_from_map(value, "status", &status) == CC_SUCCESS) {
			if (status == 200) {
				cc_log_debug("Actor '%s' migrated", (char *)msg_data);
				cc_actor_free(node, actor, false);
			} else
				cc_log_error("Failed to migrate actor '%s'", (char *)msg_data);
			return CC_SUCCESS;
		}
	}

	cc_log_error("Failed to decode message");
	return CC_FAIL;
}

static cc_result_t actor_set_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	char *value = NULL;
	bool status = false;

	if (cc_coder_get_value_from_map(data, "value", &value) == CC_SUCCESS) {
		if (cc_coder_decode_bool_from_map(value, "value", &status) == CC_SUCCESS) {
			if (status != true)
				cc_log_error("Failed to store actor '%s'", (char *)msg_data);
			return CC_SUCCESS;
		}
	}

	cc_log_error("Failed to decode data");
	return CC_FAIL;
}

void cc_actor_set_state(cc_actor_t *actor, cc_actor_state_t state)
{
	cc_log_debug("Actor '%s' state '%d' -> '%d'", actor->id, actor->state, state);
	actor->state = state;
}

static cc_result_t actor_init_from_type(cc_node_t *node, cc_actor_t *actor, char *type, uint32_t type_len)
{
	cc_actor_type_t *actor_type = NULL;

	strncpy(actor->type, type, type_len);
	actor->state = CC_ACTOR_PENDING;
	actor->in_ports = NULL;
	actor->out_ports = NULL;
	actor->instance_state = NULL;
	actor->attributes = NULL;
	actor->will_migrate = NULL;
	actor->will_end = NULL;
	actor->did_migrate = NULL;

	actor_type = (cc_actor_type_t *)cc_list_get_n(node->actor_types, type, type_len);
	if (actor_type != NULL) {
		actor->init = actor_type->init;
		actor->set_state = actor_type->set_state;
		actor->free_state = actor_type->free_state;
		actor->fire = actor_type->fire_actor;
		actor->get_managed_attributes = actor_type->get_managed_attributes;
		actor->will_migrate = actor_type->will_migrate;
		actor->will_end = actor_type->will_end;
		actor->did_migrate = actor_type->did_migrate;
		return CC_SUCCESS;
	}

#ifdef CC_PYTHON_ENABLED
	if (cc_actor_mpy_init_from_type(actor, actor->type, type_len) == CC_SUCCESS)
		return CC_SUCCESS;
#endif

	cc_log_error("Actor type '%.*s' not supported", (int)type_len, type);

	return CC_FAIL;
}

static void cc_actor_free_managed(cc_list_t *managed_attributes)
{
	cc_list_t *tmp_list = NULL;

	while (managed_attributes != NULL) {
		tmp_list = managed_attributes;
		managed_attributes = managed_attributes->next;
		cc_platform_mem_free((void *)tmp_list->id);
		cc_platform_mem_free((void *)tmp_list->data);
		cc_platform_mem_free((void *)tmp_list);
	}
}

static cc_result_t cc_actor_get_managed(char *obj_attributes, char *obj_actor_state, cc_list_t **attributes, cc_list_t **instance_attributes)
{
	cc_result_t result = CC_SUCCESS;
	char *obj_attribute_value = NULL, *attribute_name = NULL, *attribute_value = NULL, *tmp_string = NULL;
	uint32_t len = 0, array_size = 0, i_managed_attr = 0;

	array_size = cc_coder_get_size_of_array(obj_attributes);
	for (i_managed_attr = 0; i_managed_attr < array_size && result == CC_SUCCESS; i_managed_attr++) {
		result = cc_coder_decode_string_from_array(obj_attributes, i_managed_attr, &tmp_string, &len);
		if (result == CC_SUCCESS) {
			if (strncmp(tmp_string, "_id", 3) == 0 || strncmp(tmp_string, "_name", 5) == 0 || strncmp(tmp_string, "_shadow_args", 12) == 0)
				continue;

			attribute_name = NULL;
			attribute_value = NULL;

			if (cc_platform_mem_alloc((void **)&attribute_name, len + 1) != CC_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				return CC_FAIL;
			}

			strncpy(attribute_name, tmp_string, len);
			attribute_name[len] = '\0';

			if (cc_coder_get_value_from_map(obj_actor_state, attribute_name, &obj_attribute_value) != CC_SUCCESS) {
				cc_log_error("Failed to decode '%s'", attribute_name);
				cc_platform_mem_free((void *)attribute_name);
				return CC_FAIL;
			}

			len = cc_coder_get_size_of_value(obj_attribute_value);
			if (cc_platform_mem_alloc((void **)&attribute_value, len) != CC_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				cc_platform_mem_free((void *)attribute_name);
				return CC_FAIL;
			}
			memcpy(attribute_value, obj_attribute_value, len);

			if (attribute_name[0] == '_')
				result = cc_list_add(attributes, attribute_name, attribute_value, len);
			else
				result = cc_list_add(instance_attributes, attribute_name, attribute_value, len);
		}
	}

	if (result != CC_SUCCESS) {
		cc_actor_free_managed(*instance_attributes);
		*instance_attributes = NULL;
	}

	return result;
}

static cc_result_t cc_actor_get_shadow_args(char *obj_shadow_args, cc_list_t **instance_attributes)
{
	char *attributes = obj_shadow_args, *attribute_name = NULL, *attribute_value = NULL, *tmp = NULL;
	uint32_t i_attribute = 0, nbr_attributes = 0, len = 0;

	nbr_attributes = cc_coder_decode_map(&attributes);
	for (i_attribute = 0; i_attribute < nbr_attributes; i_attribute++) {
		attribute_name = NULL;
		attribute_value = NULL;

		if (cc_coder_decode_str(attributes, &tmp, &len) != CC_SUCCESS)
			return CC_FAIL;

		if (cc_platform_mem_alloc((void **)&attribute_name, len + 1) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}

		strncpy(attribute_name, tmp, len);
		attribute_name[len] = '\0';

		cc_coder_decode_map_next(&attributes);

		len = cc_coder_get_size_of_value(attributes);
		if (cc_platform_mem_alloc((void **)&attribute_value, len) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			cc_platform_mem_free((void *)attribute_name);
			return CC_FAIL;
		}
		memcpy(attribute_value, attributes, len);

		if (cc_list_add(instance_attributes, attribute_name, attribute_value, len) != CC_SUCCESS) {
			cc_platform_mem_free((void *)attribute_name);
			cc_platform_mem_free((void *)attribute_value);
			return CC_FAIL;
		}

		cc_coder_decode_map_next(&attributes);
	}

	return CC_SUCCESS;
}

static cc_result_t cc_actor_create_ports(cc_node_t *node, cc_actor_t *actor, char *obj_ports, char *obj_prev_connections, cc_port_direction_t direction)
{
	cc_result_t result = CC_SUCCESS;
	char *ports = obj_ports, *prev_connections = obj_prev_connections;
	uint32_t nbr_ports = 0, i_port = 0, nbr_keys = 0, i_key = 0;
	cc_port_t *port = NULL;

	nbr_ports = cc_coder_decode_map(&ports);
	for (i_port = 0; i_port < nbr_ports && result == CC_SUCCESS; i_port++) {
		cc_coder_decode_map_next(&ports);

		port = cc_port_create(node, actor, ports, prev_connections, direction);
		if (port == NULL)
			return CC_FAIL;

		if (direction == CC_PORT_DIRECTION_IN)
			result = cc_list_add(&actor->in_ports, port->id, (void *)port, sizeof(cc_port_t));
		else
			result = cc_list_add(&actor->out_ports, port->id, (void *)port, sizeof(cc_port_t));

		nbr_keys = cc_coder_decode_map(&ports);
		for (i_key = 0; i_key < nbr_keys; i_key++) {
			cc_coder_decode_map_next(&ports);
			cc_coder_decode_map_next(&ports);
		}
	}

	return result;
}

cc_actor_t *cc_actor_create(cc_node_t *node, char *root)
{
	cc_actor_t *actor = NULL;
	char *type = NULL, *obj_state = NULL, *obj_actor_state = NULL, *obj_prev_connections = NULL;
	char *obj_ports = NULL, *obj_managed = NULL, *obj_shadow_args = NULL, *r = root, *id = NULL, *name = NULL;
	uint32_t type_len = 0, id_len = 0, name_len = 0;
	cc_list_t *instance_attributes = NULL;

	if (cc_platform_mem_alloc((void **)&actor, sizeof(cc_actor_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	memset(actor, 0, sizeof(cc_actor_t));

	actor->calvinsys = node->calvinsys;

	if (cc_coder_get_value_from_map(r, "state", &obj_state) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (cc_coder_decode_string_from_map(obj_state, "actor_type", &type, &type_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'actor_type'");
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (actor_init_from_type(node, actor, type, type_len) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (cc_list_add(&node->actors, actor->id, (void *)actor, sizeof(cc_actor_t)) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (cc_coder_get_value_from_map(obj_state, "actor_state", &obj_actor_state) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (cc_coder_decode_string_from_map(obj_actor_state, "_id", &id, &id_len) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}
	strncpy(actor->id, id, id_len);

	if (cc_coder_decode_string_from_map(obj_actor_state, "_name", &name, &name_len) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}
	strncpy(actor->name, name, name_len);

	if (cc_coder_get_value_from_map(obj_state, "prev_connections", &obj_prev_connections) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (cc_coder_get_value_from_map(obj_actor_state, "inports", &obj_ports) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (cc_actor_create_ports(node, actor, obj_ports, obj_prev_connections, CC_PORT_DIRECTION_IN) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (cc_coder_get_value_from_map(obj_actor_state, "outports", &obj_ports) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (cc_actor_create_ports(node, actor, obj_ports, obj_prev_connections, CC_PORT_DIRECTION_OUT) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (cc_coder_get_value_from_map(obj_actor_state, "_managed", &obj_managed) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (cc_actor_get_managed(obj_managed, obj_actor_state, &actor->attributes, &instance_attributes) != CC_SUCCESS) {
		cc_actor_free(node, actor, false);
		return NULL;
	}

	if (cc_coder_has_key(obj_actor_state, "_shadow_args")) {
		if (actor->init != NULL) {
			if (cc_coder_get_value_from_map(obj_actor_state, "_shadow_args", &obj_shadow_args) != CC_SUCCESS) {
				cc_actor_free(node, actor, false);
				return NULL;
			}

			if (cc_actor_get_shadow_args(obj_shadow_args, &instance_attributes) != CC_SUCCESS) {
				cc_actor_free(node, actor, false);
				return NULL;
			}

			if (actor->init(&actor, instance_attributes) != CC_SUCCESS) {
				cc_actor_free_managed(instance_attributes);
				cc_actor_free(node, actor, false);
				return NULL;
			}
		}
	} else {
		if (actor->set_state != NULL && actor->set_state(&actor, instance_attributes) != CC_SUCCESS) {
			cc_actor_free_managed(instance_attributes);
			cc_actor_free(node, actor, false);
			return NULL;
		}
		if (actor->did_migrate != NULL)
			actor->did_migrate(actor);
	}

	if (node->transport_client != NULL) {
		if (cc_proto_send_set_actor(node, actor, actor_set_reply_handler) != CC_SUCCESS) {
			cc_actor_free_managed(instance_attributes);
			cc_actor_free(node, actor, false);
			return NULL;
		}
	}

	// attributes should now be handled by the instance
	cc_actor_free_managed(instance_attributes);

	cc_log("Actor '%s' created with id '%s' of type '%s'", actor->name, actor->id, actor->type);

	return actor;
}

void cc_actor_free(cc_node_t *node, cc_actor_t *actor, bool remove_from_registry)
{
	cc_list_t *list = NULL, *tmp_list = NULL;

	cc_log("Deleting actor with id '%s'", actor->id);

	if (actor->will_end != NULL)
		actor->will_end(actor);

	if (remove_from_registry) {
		if (cc_proto_send_remove_actor(node, actor, actor_remove_reply_handler) != CC_SUCCESS)
			cc_log_error("Failed to remove actor '%s'", actor->id);
	}

	if (actor->instance_state != NULL && actor->free_state != NULL)
		actor->free_state(actor);

	list = actor->in_ports;
	while (list != NULL) {
		tmp_list = list;
		list = list->next;
		cc_port_free(node, (cc_port_t *)tmp_list->data, remove_from_registry);
		cc_platform_mem_free((void *)tmp_list);
	}

	list = actor->out_ports;
	while (list != NULL) {
		tmp_list = list;
		list = list->next;
		cc_port_free(node, (cc_port_t *)tmp_list->data, remove_from_registry);
		cc_platform_mem_free((void *)tmp_list);
	}

	cc_list_remove(&node->actors, actor->id);
	cc_actor_free_managed(actor->attributes);
	cc_platform_mem_free((void *)actor);
	actor = NULL;
}

cc_actor_t *cc_actor_get(cc_node_t *node, const char *actor_id, uint32_t actor_id_len)
{
	return (cc_actor_t *)cc_list_get(node->actors, actor_id);
}

void cc_actor_CC_PORT_ENABLED(cc_actor_t *actor)
{
	cc_list_t *list = NULL;

	list = actor->in_ports;
	while (list != NULL) {
		if (((cc_port_t *)list->data)->state != CC_PORT_ENABLED)
			return;
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		if (((cc_port_t *)list->data)->state != CC_PORT_ENABLED)
			return;
		list = list->next;
	}

	cc_actor_set_state(actor, CC_ACTOR_ENABLED);
}

void cc_actor_CC_PORT_DISCONNECTED(cc_actor_t *actor)
{
	cc_actor_set_state(actor, CC_ACTOR_PENDING);
}

void cc_actor_disconnect(cc_node_t *node, cc_actor_t *actor)
{
	cc_list_t *list = NULL;

	list = actor->in_ports;
	while (list != NULL) {
		cc_port_disconnect(node, (cc_port_t *)list->data);
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		cc_port_disconnect(node, (cc_port_t *)list->data);
		list = list->next;
	}

	cc_actor_set_state(actor, CC_ACTOR_PENDING);
}

cc_result_t cc_actor_migrate(cc_node_t *node, cc_actor_t *actor, char *to_rt_uuid, uint32_t to_rt_uuid_len)
{
	cc_list_t *list = NULL;

	if (actor->will_migrate != NULL)
		actor->will_migrate(actor);

	list = actor->in_ports;
	while (list != NULL) {
		cc_port_disconnect(node, (cc_port_t *)list->data);
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		cc_port_disconnect(node, (cc_port_t *)list->data);
		list = list->next;
	}

	if (cc_proto_send_actor_new(node, actor, to_rt_uuid, to_rt_uuid_len, cc_actor_migrate_reply_handler) == CC_SUCCESS)
		return CC_SUCCESS;

	cc_log_error("Failed to migrate actor '%s'", actor->id);
	return CC_FAIL;
}

char *cc_actor_serialize(const cc_node_t *node, const cc_actor_t *actor, char *buffer, bool include_state)
{
	cc_list_t *in_ports = NULL, *out_ports = NULL;
	cc_list_t *instance_attributes = NULL, *tmp_list = NULL;
	cc_port_t *port = NULL;
	unsigned int nbr_state_attributes = 3, nbr_port_attributes = 4;
	unsigned int nbr_inports = 0, nbr_outports = 0, i_token = 0, nbr_managed_attributes = 0;
	char *peer_id = NULL;

	if (actor->get_managed_attributes != NULL) {
		if (actor->get_managed_attributes((cc_actor_t *)actor, &instance_attributes) != CC_SUCCESS)
			cc_log_error("Failed to get instance attributes");
	}

	nbr_managed_attributes = 2; // _id/_name
	nbr_managed_attributes += cc_list_count(actor->attributes);
	if (instance_attributes != NULL)
		nbr_managed_attributes += cc_list_count(instance_attributes);
	in_ports = actor->in_ports;
	nbr_inports = cc_list_count(in_ports);
	out_ports = actor->out_ports;
	nbr_outports = cc_list_count(out_ports);

	if (include_state) {
		nbr_state_attributes += 1;
		nbr_port_attributes += 1;
	}

	buffer = cc_coder_encode_kv_map(buffer, "state", nbr_state_attributes);
	{
		if (include_state)
			buffer = cc_coder_encode_kv_uint(buffer, "constrained_state", actor->state);
		buffer = cc_coder_encode_kv_str(buffer, "actor_type", actor->type, strlen(actor->type));
		buffer = cc_coder_encode_kv_map(buffer, "prev_connections", 2);
		{
			buffer = cc_coder_encode_kv_map(buffer, "inports", nbr_inports);
			in_ports = actor->in_ports;
			while (in_ports != NULL) {
				port = (cc_port_t *)in_ports->data;
				peer_id = cc_port_get_peer_id(node, port);
				buffer = cc_coder_encode_str(buffer, port->id, strnlen(port->id, CC_UUID_BUFFER_SIZE));
				buffer = cc_coder_encode_array(buffer, 1);
				buffer = cc_coder_encode_array(buffer, 2);
				if (peer_id != NULL)
					buffer = cc_coder_encode_str(buffer, peer_id, strnlen(peer_id, CC_UUID_BUFFER_SIZE));
				else
					buffer = cc_coder_encode_nil(buffer);
				buffer = cc_coder_encode_str(buffer, port->peer_port_id, strnlen(port->peer_port_id, CC_UUID_BUFFER_SIZE));
				in_ports = in_ports->next;
			}

			buffer = cc_coder_encode_kv_map(buffer, "outports", nbr_outports);
			out_ports = actor->out_ports;
			while (out_ports != NULL) {
				port = (cc_port_t *)out_ports->data;
				peer_id = cc_port_get_peer_id(node, port);
				buffer = cc_coder_encode_str(buffer, port->id, strnlen(port->id, CC_UUID_BUFFER_SIZE));
				buffer = cc_coder_encode_array(buffer, 1);
				buffer = cc_coder_encode_array(buffer, 2);
				if (peer_id != NULL)
					buffer = cc_coder_encode_str(buffer, peer_id, strnlen(peer_id, CC_UUID_BUFFER_SIZE));
				else
					buffer = cc_coder_encode_nil(buffer);
				buffer = cc_coder_encode_str(buffer, port->peer_port_id, strnlen(port->peer_port_id, CC_UUID_BUFFER_SIZE));
				out_ports = out_ports->next;
			}
		}

		buffer = cc_coder_encode_kv_map(buffer, "actor_state", nbr_managed_attributes + 4);
		{
			buffer = cc_coder_encode_kv_array(buffer, "_component_members", 1);
			{
				buffer = cc_coder_encode_str(buffer, actor->id, strnlen(actor->id, CC_UUID_BUFFER_SIZE));
			}

			buffer = cc_coder_encode_kv_array(buffer, "_managed", nbr_managed_attributes);
			{
				buffer = cc_coder_encode_str(buffer, "_id", 3);
				buffer = cc_coder_encode_str(buffer, "_name", 5);

				tmp_list = actor->attributes;
				while (tmp_list != NULL) {
					buffer = cc_coder_encode_str(buffer, tmp_list->id, strlen(tmp_list->id));
					tmp_list = tmp_list->next;
				}

				tmp_list = instance_attributes;
				while (tmp_list != NULL) {
					buffer = cc_coder_encode_str(buffer, tmp_list->id, strlen(tmp_list->id));
					tmp_list = tmp_list->next;
				}
			}

			{
				buffer = cc_coder_encode_kv_str(buffer, "_id", actor->id, strnlen(actor->id, CC_UUID_BUFFER_SIZE));
				buffer = cc_coder_encode_kv_str(buffer, "_name", actor->name, strlen(actor->name));

				tmp_list = actor->attributes;
				while (tmp_list != NULL) {
					buffer = cc_coder_encode_kv_value(buffer, tmp_list->id, (char *)tmp_list->data, tmp_list->data_len);
					tmp_list = tmp_list->next;
				}

				tmp_list = instance_attributes;
				while (tmp_list != NULL) {
					buffer = cc_coder_encode_kv_value(buffer, tmp_list->id, (char *)tmp_list->data, tmp_list->data_len);
					tmp_list = tmp_list->next;
				}
				cc_actor_free_managed(instance_attributes);
			}

			buffer = cc_coder_encode_kv_map(buffer, "inports", nbr_inports);
			{
				in_ports = actor->in_ports;
				while (in_ports != NULL) {
					port = (cc_port_t *)in_ports->data;
					buffer = cc_coder_encode_kv_map(buffer, port->name, nbr_port_attributes);
					{
						if (include_state)
							buffer = cc_coder_encode_kv_uint(buffer, "constrained_state", port->state);
						buffer = cc_coder_encode_kv_str(buffer, "id", port->id, strnlen(port->id, CC_UUID_BUFFER_SIZE));
						buffer = cc_coder_encode_kv_str(buffer, "name", port->name, strlen(port->name));
						buffer = cc_coder_encode_kv_map(buffer, "queue", 7);
						{
							buffer = cc_coder_encode_kv_str(buffer, "queuetype", "fanout_fifo", 11);
							buffer = cc_coder_encode_kv_uint(buffer, "write_pos", port->fifo->write_pos);
							buffer = cc_coder_encode_kv_array(buffer, "readers", 1);
							{
								buffer = cc_coder_encode_str(buffer, port->id, strnlen(port->id, CC_UUID_BUFFER_SIZE));
							}
							buffer = cc_coder_encode_kv_uint(buffer, "N", port->fifo->size);
							buffer = cc_coder_encode_kv_map(buffer, "tentative_read_pos", 1);
							{
								buffer = cc_coder_encode_kv_uint(buffer, port->id, port->fifo->tentative_read_pos);
							}
							buffer = cc_coder_encode_kv_map(buffer, "read_pos", 1);
							{
								buffer = cc_coder_encode_kv_uint(buffer, port->id, port->fifo->read_pos);
							}
							buffer = cc_coder_encode_kv_array(buffer, "fifo", port->fifo->size);
							{
								for (i_token = 0; i_token < port->fifo->size; i_token++)
									buffer = cc_token_encode(buffer, &port->fifo->tokens[i_token], false);
							}
						}
						buffer = cc_coder_encode_kv_map(buffer, "properties", 3);
						{
							buffer = cc_coder_encode_kv_uint(buffer, "nbr_peers", 1);
							buffer = cc_coder_encode_kv_str(buffer, "direction", "in", 2);
							buffer = cc_coder_encode_kv_str(buffer, "routing", "default", 7);
						}
					}
					in_ports = in_ports->next;
				}
			}
			buffer = cc_coder_encode_kv_map(buffer, "outports", nbr_outports);
			{
				out_ports = actor->out_ports;
				while (out_ports != NULL) {
					port = (cc_port_t *)out_ports->data;
					buffer = cc_coder_encode_kv_map(buffer, port->name, nbr_port_attributes);
					{
						if (include_state)
							buffer = cc_coder_encode_kv_uint(buffer, "constrained_state", port->state);
						buffer = cc_coder_encode_kv_str(buffer, "id", port->id, strnlen(port->id, CC_UUID_BUFFER_SIZE));
						buffer = cc_coder_encode_kv_str(buffer, "name", port->name, strnlen(port->name, CC_UUID_BUFFER_SIZE));
						buffer = cc_coder_encode_kv_map(buffer, "queue", 7);
						{
							buffer = cc_coder_encode_kv_str(buffer, "queuetype", "fanout_fifo", 11);
							buffer = cc_coder_encode_kv_uint(buffer, "write_pos", port->fifo->write_pos);
							buffer = cc_coder_encode_kv_array(buffer, "readers", 1);
							{
								buffer = cc_coder_encode_str(buffer, port->peer_port_id, strnlen(port->peer_port_id, CC_UUID_BUFFER_SIZE));
							}
							buffer = cc_coder_encode_kv_uint(buffer, "N", port->fifo->size);
							buffer = cc_coder_encode_kv_map(buffer, "tentative_read_pos", 1);
							{
								buffer = cc_coder_encode_kv_uint(buffer, port->peer_port_id, port->fifo->tentative_read_pos);
							}
							buffer = cc_coder_encode_kv_map(buffer, "read_pos", 1);
							{
								buffer = cc_coder_encode_kv_uint(buffer, port->peer_port_id, port->fifo->read_pos);
							}
							buffer = cc_coder_encode_kv_array(buffer, "fifo", port->fifo->size);
							{
								for (i_token = 0; i_token < port->fifo->size; i_token++)
									buffer = cc_token_encode(buffer, &port->fifo->tokens[i_token], false);
							}
						}
						buffer = cc_coder_encode_kv_map(buffer, "properties", 3);
						{
							buffer = cc_coder_encode_kv_uint(buffer, "nbr_peers", 1);
							buffer = cc_coder_encode_kv_str(buffer, "direction", "out", 3);
							buffer = cc_coder_encode_kv_str(buffer, "routing", "default", 7);
						}
					}
					out_ports = out_ports->next;
				}
			}
		}
	}

	return buffer;
}
