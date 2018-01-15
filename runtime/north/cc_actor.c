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
#include "cc_config.h"
#include "cc_node.h"
#include "cc_actor.h"
#include "cc_actor_store.h"
#include "cc_fifo.h"
#include "cc_token.h"
#include "runtime/south/platform/cc_platform.h"
#include "coder/cc_coder.h"
#include "cc_proto.h"
#if CC_USE_PYTHON
#include <stdio.h>
#include "libmpy/cc_actor_mpy.h"
#endif

static void cc_actor_free_attribute_list(cc_list_t *managed_attributes);

cc_result_t cc_actor_req_match_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	char *value = NULL, *obj_data = NULL, *actor_id = NULL, *possible_placements = NULL, *peer_id = NULL;
	uint32_t actor_id_len = 0, peer_id_len = 0, status = 0;
	cc_actor_t *actor = NULL;

	if (cc_coder_get_value_from_map(data, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'value'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(value, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'value'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(value, "data", &obj_data) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'data'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(value, "status", &status) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'status'");
		return CC_FAIL;
	}

	if (status != 200) {
		cc_log("Failed to find match");
		return CC_SUCCESS;
	}

	if (cc_coder_decode_string_from_map(obj_data, "actor_id", &actor_id, &actor_id_len) != CC_SUCCESS) {
		cc_log_error("Failed to get 'actor_id'");
		return CC_FAIL;
	}

	actor = cc_actor_get(node, actor_id, actor_id_len);
	if (actor == NULL) {
		cc_log_error("No actor");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(obj_data, "possible_placements", &possible_placements) != CC_SUCCESS) {
		cc_log_error("Failed to get 'possible_placements'");
		return CC_FAIL;
	}

	if (cc_coder_type_of(possible_placements) != CC_CODER_ARRAY) {
		cc_log_error("'possible_placements' is not an array");
		return CC_FAIL;
	}

	if (cc_coder_get_size_of_array(possible_placements) > 0) {
		if (cc_coder_decode_string_from_array(possible_placements, 0, &peer_id, &peer_id_len) != CC_SUCCESS) {
			cc_log_error("Failed to decode 'possible_placements'");
			return CC_FAIL;
		}

		return cc_actor_migrate(node, actor, peer_id, peer_id_len);
	}

	return CC_SUCCESS;
}

static cc_result_t actor_remove_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	return CC_SUCCESS;
}

static cc_result_t cc_actor_migrate_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	uint32_t status = 0, actor_id_len = 0;
	char *value = NULL, *actor_id = NULL;
	cc_actor_t *actor = NULL;

	actor_id = (char *)msg_data;
	actor_id_len = strnlen(actor_id, CC_UUID_BUFFER_SIZE);

	if (cc_coder_get_value_from_map(data, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to get 'value'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(value, "status", &status) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'status'");
		return CC_FAIL;
	}

	actor = cc_actor_get(node, actor_id, actor_id_len);
	if (actor == NULL) {
		cc_log_error("No actor with id '%s'", actor_id);
		return CC_FAIL;
	}

	if (status == 200) {
		cc_log("Actor: '%s' migrated", actor_id);
		cc_actor_free(node, actor, false);
		return CC_SUCCESS;
	}

	cc_log_error("Failed to migrate actor '%s'", actor_id);
	return CC_FAIL;
}

static cc_result_t actor_set_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	char *value = NULL, *response = NULL, *key = NULL;
	uint32_t key_len = 0, status = 0;
	cc_actor_t *actor = NULL;

	if (cc_coder_get_value_from_map(data, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to get 'value'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(value, "key", &key, &key_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'key'");
		return CC_FAIL;
	}

	if (strncmp(key, "actor-", 6) != 0) {
		cc_log_error("Unexpected key");
		return CC_FAIL;
	}

	actor = cc_actor_get(node, key + 6, key_len - 6);
	if (actor == NULL)
		return CC_SUCCESS;

	if (cc_coder_get_value_from_map(value, "response", &response) != CC_SUCCESS) {
		cc_log_error("Failed to get 'response'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(response, "status", &status) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'status'");
		return CC_FAIL;
	}

	if (status == 200)
		cc_log_debug("Stored '%s'", actor->id);
	else
		cc_log_error("Failed to store '%s'", actor->id);

	return CC_SUCCESS;
}

void cc_actor_set_state(cc_actor_t *actor, cc_actor_state_t state)
{
	cc_log_debug("Actor '%s' state '%d' -> '%d'", actor->id, actor->state, state);
	if (state == CC_ACTOR_ENABLED && actor->state != CC_ACTOR_ENABLED)
		cc_log("Actor: Enabled '%s'", actor->id);
	actor->state = state;
}

#if CC_USE_PYTHON
static void cc_actor_store_module(char *type, uint32_t type_len, char *data, uint32_t data_len)
{
	char *path = NULL;

	path = cc_actor_mpy_get_path_from_type(type, type_len, ".mpy", true);
	if (path != NULL) {
		if (cc_platform_file_write(path, data, data_len) == CC_SUCCESS)
			cc_log("Actor: '%s' written", path);
		else
			cc_log_error("Failed to write '%s'", path);
		cc_platform_mem_free(path);
	}
}

static void cc_actor_update_pending(cc_node_t *node, char *type, uint32_t type_len, uint32_t status)
{
	cc_list_t *item = NULL;
	cc_actor_t *actor = NULL;
	cc_result_t result = CC_FAIL;

	item = node->actors;
	while (item != NULL) {
		actor = (cc_actor_t*)item->data;
		if (actor->state == CC_ACTOR_PENDING_IMPL && strncmp(actor->type, type, type_len) == 0) {
			if (status == 200) {
				result = cc_actor_mpy_init_from_type(actor);
				if (result == CC_SUCCESS) {
					if (actor->was_shadow)
						result = actor->init(&actor, actor->managed_attributes);
					else {
						result = actor->set_state(&actor, actor->managed_attributes);
						if (result == CC_SUCCESS && actor->did_migrate != NULL)
							actor->did_migrate(actor);
					}
				}
			} else
				result = CC_FAIL;

			if (actor->managed_attributes != NULL) {
				cc_actor_free_attribute_list(actor->managed_attributes);
				actor->managed_attributes = NULL;
			}

			if (result == CC_SUCCESS) {
				cc_log("Actor: Loaded '%s'", actor->id);
				cc_actor_connect_ports(node, actor);
			} else {
				cc_log_error("Failed to load '%s'", actor->id);
				if (cc_actor_migrate(node, actor, node->proxy_link->peer_id, strlen(node->proxy_link->peer_id)) != CC_SUCCESS)
					cc_actor_free(node, actor, true);
			}
		}
		item = item->next;
	}
}

static cc_result_t cc_actor_get_compiled_actor_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	uint32_t status = 0, type_len = 0, module_len = 0;
	char *value = NULL, *module = NULL, *type = NULL, *obj_data = NULL;

	if (cc_coder_get_value_from_map(data, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'value'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(value, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'value'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(value, "status", &status) != CC_SUCCESS) {
		cc_log_error("Failed decode 'status'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(value, "data", &obj_data) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'data'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(obj_data, "actor_type", &type, &type_len) != CC_SUCCESS) {
		cc_log_error("Failed to get 'actor_id'");
		return CC_FAIL;
	}

	if (status == 200) {
		if (cc_coder_decode_string_from_map(obj_data, "module", &module, &module_len) == CC_SUCCESS)
			cc_actor_store_module(type, type_len, module, module_len);
		else
			cc_log_error("Failed get 'module'");
	}

	cc_actor_update_pending(node, type, type_len, status);

	return CC_SUCCESS;
}
#endif

static cc_actor_t *cc_actor_create_from_type(cc_node_t *node, char *type, uint32_t type_len)
{
	cc_actor_t *actor = NULL;
	cc_list_t *item = NULL;
	cc_actor_type_t *actor_type = NULL;

	if (cc_platform_mem_alloc((void **)&actor, sizeof(cc_actor_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	memset(actor, 0, sizeof(cc_actor_t));
	actor->state = CC_ACTOR_PENDING;
	actor->calvinsys = node->calvinsys;

	if (cc_platform_mem_alloc((void **)&actor->type, type_len + 1) != CC_SUCCESS) {
		cc_log_error("Failed to alllocate memory");
		cc_platform_mem_free((void *)actor);
		return NULL;
	}
	strncpy(actor->type, type, type_len);
	actor->type[type_len] = '\0';

	item = cc_list_get_n(node->actor_types, type, type_len);
	if (item != NULL) {
		actor_type = (cc_actor_type_t *)item->data;
		actor->init = actor_type->init;
		actor->set_state = actor_type->set_state;
		actor->free_state = actor_type->free_state;
		actor->fire = actor_type->fire_actor;
		actor->get_managed_attributes = actor_type->get_managed_attributes;
		actor->will_migrate = actor_type->will_migrate;
		actor->will_end = actor_type->will_end;
		actor->did_migrate = actor_type->did_migrate;
		actor->get_requires = actor_type->get_requires;
		actor->requires = actor_type->requires;
		return actor;
	}

#if CC_USE_PYTHON
	if (cc_actor_mpy_has_module(actor->type)) {
		if (cc_actor_mpy_init_from_type(actor) == CC_SUCCESS)
			return actor;
	} else {
		actor->state= CC_ACTOR_PENDING_IMPL;
		if (cc_proto_send_get_actor_module(node, actor->type, cc_actor_get_compiled_actor_reply_handler) == CC_SUCCESS)
			return actor;
	}
#endif

	cc_log_error("Failed to load '%s'", actor->type);
	cc_platform_mem_free((void *)actor->type);
	cc_platform_mem_free((void *)actor);

	return NULL;
}

static void cc_actor_free_attribute_list(cc_list_t *attributes)
{
	cc_list_t *tmp_list = NULL;

	while (attributes != NULL) {
		tmp_list = attributes;
		attributes = attributes->next;
		cc_platform_mem_free((void *)tmp_list->id);
		cc_platform_mem_free((void *)tmp_list->data);
		cc_platform_mem_free((void *)tmp_list);
	}
}

static cc_result_t cc_actor_get_attributes(cc_actor_t *actor, char *obj_managed, cc_list_t **attributes, bool private_only)
{
	cc_result_t result = CC_SUCCESS;
	char *managed = obj_managed, *key = NULL, *data = NULL;
	uint32_t index = 0, key_len = 0, value_len = 0, map_size = 0;
	char *tmp = NULL;
	uint32_t tmp_len = 0;

	map_size = cc_coder_decode_map(&managed);
	for (index = 0; index < map_size; index++) {
		if (cc_coder_decode_str(managed, &key, &key_len) != CC_SUCCESS) {
			cc_log_error("Failed to decode key");
			return CC_FAIL;
		}

		if (private_only && key[0] != '_') {
			cc_coder_decode_map_next(&managed);
			cc_coder_decode_map_next(&managed);
			continue;
		}

		if (strncmp(key, "_shadow_args", 12) == 0) {
			cc_coder_decode_map_next(&managed);
			cc_coder_decode_map_next(&managed);
			continue;
		}

		cc_coder_decode_map_next(&managed);

		value_len = cc_coder_get_size_of_value(managed);
		if (cc_platform_mem_alloc((void **)&data, value_len) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			result = CC_FAIL;
			break;
		}
		memcpy(data, managed, value_len);
		cc_coder_decode_map_next(&managed);

		if (strncmp(key, "_id", 3) == 0) {
			if (cc_coder_decode_str(data, &tmp, &tmp_len) != CC_SUCCESS) {
				cc_log_error("Failed to decode 'id'");
				result = CC_FAIL;
				break;
			}

			if (cc_platform_mem_alloc((void **)&actor->id, tmp_len + 1) != CC_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				result = CC_FAIL;
				break;
			}
			strncpy(actor->id, tmp, tmp_len);
			actor->id[tmp_len] = '\0';
		}

		if (strncmp(key, "_name", 5) == 0) {
			if (cc_coder_decode_str(data, &tmp, &tmp_len) != CC_SUCCESS) {
				cc_log_error("Failed to decode 'name'");
				result = CC_FAIL;
				break;
			}

			if (cc_platform_mem_alloc((void **)&actor->name, tmp_len + 1) != CC_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				result = CC_FAIL;
				break;
			}
			strncpy(actor->name, tmp, tmp_len);
			actor->name[tmp_len] = '\0';
		}

		if (cc_list_add_n(attributes, key, key_len, data, value_len) == NULL) {
			cc_log_error("Failed to add attribute");
			result = CC_FAIL;
			break;
		}
	}

	if (result != CC_SUCCESS) {
		cc_actor_free_attribute_list(*attributes);
		*attributes = NULL;
	}

	return result;
}

static cc_result_t cc_actor_create_ports(cc_node_t *node, cc_actor_t *actor, char *obj_ports, char *obj_prev_connections, cc_port_direction_t direction)
{
	char *ports = obj_ports, *prev_connections = obj_prev_connections;
	uint32_t nbr_ports = 0, i_port = 0, nbr_keys = 0, i_key = 0;
	cc_port_t *port = NULL;

	nbr_ports = cc_coder_decode_map(&ports);
	for (i_port = 0; i_port < nbr_ports; i_port++) {
		cc_coder_decode_map_next(&ports);

		port = cc_port_create(node, actor, ports, prev_connections, direction);
		if (port == NULL)
			return CC_FAIL;

		if (direction == CC_PORT_DIRECTION_IN) {
			if (cc_list_add(&actor->in_ports, port->id, (void *)port, sizeof(cc_port_t)) == NULL) {
				cc_log_error("Failed to add port");
				return CC_FAIL;
			}
		} else {
			if (cc_list_add(&actor->out_ports, port->id, (void *)port, sizeof(cc_port_t)) == NULL) {
				cc_log_error("Failed to add port");
				return CC_FAIL;
			}
		}

		nbr_keys = cc_coder_decode_map(&ports);
		for (i_key = 0; i_key < nbr_keys; i_key++) {
			cc_coder_decode_map_next(&ports);
			cc_coder_decode_map_next(&ports);
		}
	}

	return CC_SUCCESS;
}

void cc_actor_connect_ports(cc_node_t *node, cc_actor_t *actor)
{
	cc_list_t *list = NULL;
	cc_port_t *port = NULL;
	cc_actor_state_t state = CC_ACTOR_ENABLED;

	list = actor->in_ports;
	while (list != NULL) {
		port = (cc_port_t *)list->data;
		if (port->state != CC_PORT_ENABLED) {
			cc_port_connect(node, port);
			state = CC_ACTOR_PENDING;
		}
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		port = (cc_port_t *)list->data;
		if (port->state != CC_PORT_ENABLED) {
			cc_port_connect(node, port);
			state = CC_ACTOR_PENDING;
		}
		list = list->next;
	}

	cc_actor_set_state(actor, state);
}

cc_actor_t *cc_actor_create(cc_node_t *node, char *root)
{
	cc_result_t result = CC_SUCCESS;
	cc_actor_t *actor = NULL;
	char *obj_state = NULL, *obj_actor_state = NULL, *obj_prev_connections = NULL;
	char *obj_ports = NULL, *obj_private = NULL, *obj_managed = NULL, *obj_shadow_args = NULL;
	char *obj_calvinsys = NULL, *id = NULL, *actor_type = NULL, *r = root;
	char *obj_replication_data = NULL, *replication_master = NULL;
	uint32_t id_len = 0, actor_type_len = 0, replication_master_len = 0;
	cc_list_t *item = NULL;

	if (result == CC_SUCCESS && cc_coder_get_value_from_map(r, "state", &obj_state) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'state'");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && cc_coder_decode_string_from_map(obj_state, "actor_type", &actor_type, &actor_type_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'actor_type'");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS) {
	 	actor = cc_actor_create_from_type(node, actor_type, actor_type_len);
		if (actor == NULL)
			return NULL;
	}

	if (result == CC_SUCCESS && cc_coder_get_value_from_map(obj_state, "actor_state", &obj_actor_state) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'actor_state'");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && cc_coder_get_value_from_map(obj_actor_state, "private", &obj_private) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'private'");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && cc_coder_has_key(obj_private, "_calvinsys")) {
		if (cc_coder_get_value_from_map(obj_private, "_calvinsys", &obj_calvinsys) != CC_SUCCESS) {
			cc_log_error("Failed to get '_calvinsys'");
			result = CC_FAIL;
		} else
			result = cc_calvinsys_deserialize(actor, obj_calvinsys);
	}

	if (result == CC_SUCCESS && cc_coder_decode_string_from_map(obj_private, "_id", &id, &id_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode '_id'");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && cc_actor_get_attributes(actor, obj_private, &actor->private_attributes, true) != CC_SUCCESS) {
		cc_log_error("Failed to get private attributes");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && cc_coder_get_value_from_map(obj_state, "prev_connections", &obj_prev_connections) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'prev_connections'");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS) {
		if (cc_coder_get_value_from_map(obj_private, "_replication_data", &obj_replication_data) == CC_SUCCESS) {
			if (cc_coder_decode_string_from_map(obj_replication_data, "master", &replication_master, &replication_master_len) == CC_SUCCESS) {
				if (replication_master_len == id_len && strncmp(id, replication_master, id_len) == 0) {
					cc_log_error("Replication masters not supported");
					result = CC_FAIL;
				}
			}
		}
	}

	if (result == CC_SUCCESS && cc_coder_get_value_from_map(obj_private, "inports", &obj_ports) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'inports'");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && cc_actor_create_ports(node, actor, obj_ports, obj_prev_connections, CC_PORT_DIRECTION_IN) != CC_SUCCESS) {
		cc_log_error("Failed to create inports");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && cc_coder_get_value_from_map(obj_private, "outports", &obj_ports) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'outports'");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && cc_actor_create_ports(node, actor, obj_ports, obj_prev_connections, CC_PORT_DIRECTION_OUT) != CC_SUCCESS) {
		cc_log_error("Failed to create outports");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && cc_coder_get_value_from_map(obj_actor_state, "managed", &obj_managed) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'managed'");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && cc_actor_get_attributes(actor, obj_managed, &actor->managed_attributes, false) != CC_SUCCESS) {
		cc_log_error("Failed to get managed attributes");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && actor->id == NULL) {
		cc_log_error("Failed to get '_id'");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS && actor->name == NULL) {
		cc_log_error("Failed to get '_name'");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS) {
		if (cc_coder_has_key(obj_managed, "_shadow_args")) {
			actor->was_shadow = true;

			if (cc_coder_get_value_from_map(obj_managed, "_shadow_args", &obj_shadow_args) != CC_SUCCESS) {
				cc_log_error("Failed to decode '_shadow_args'");
				result = CC_FAIL;
			}

			if (result == CC_SUCCESS && cc_actor_get_attributes(actor, obj_shadow_args, &actor->managed_attributes, false) != CC_SUCCESS) {
				cc_log_error("Failed to get shadow args");
				result = CC_FAIL;
			}

			if (result == CC_SUCCESS && actor->state != CC_ACTOR_PENDING_IMPL)
			 	result = actor->init(&actor, actor->managed_attributes);
		} else {
			actor->was_shadow = false;

			if (actor->state != CC_ACTOR_PENDING_IMPL) {
				result = actor->set_state(&actor, actor->managed_attributes);
				if (result == CC_SUCCESS && actor->did_migrate != NULL)
					actor->did_migrate(actor);
			}
		}
	}

	if (result == CC_SUCCESS && node->transport_client != NULL) {
		if (cc_proto_send_set_actor(node, actor, actor_set_reply_handler) != CC_SUCCESS) {
			cc_log_error("Failed add actor to registry");
			result = CC_FAIL;
		}
	}

	if (actor->managed_attributes != NULL && (result != CC_SUCCESS || actor->state != CC_ACTOR_PENDING_IMPL))
		cc_actor_free_attribute_list(actor->managed_attributes);

	if (actor != NULL && actor->private_attributes != NULL) {
		// _calvinsys is written when the actor is serialized
		item = cc_list_get(actor->private_attributes, "_calvinsys");
		if (item != NULL) {
			cc_platform_mem_free(item->data);
			cc_list_remove(&actor->private_attributes, "_calvinsys");
		}

		// inports is written when the actor is serialized
		item = cc_list_get(actor->private_attributes, "inports");
		if (item != NULL) {
			cc_platform_mem_free(item->data);
			cc_list_remove(&actor->private_attributes, "inports");
		}

		// outports is written when the actor is serialized
		item = cc_list_get(actor->private_attributes, "outports");
		if (item != NULL) {
			cc_platform_mem_free(item->data);
			cc_list_remove(&actor->private_attributes, "outports");
		}
	}

	if (result == CC_SUCCESS && cc_list_add(&node->actors, actor->id, (void *)actor, sizeof(cc_actor_t)) == NULL) {
		cc_log_error("Failed to add actor");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS) {
		if (actor->state == CC_ACTOR_PENDING_IMPL)
			cc_log("Actor: Created '%s', type '%s' with pending implementation", actor->id, actor->type);
		else {
			cc_log("Actor: Created '%s', type '%s'", actor->id, actor->type);
			cc_actor_connect_ports(node, actor);
		}
	} else {
		cc_log_error("Failed to create actor");
		cc_actor_free(node, actor, false);
		actor = NULL;
	}

	return actor;
}

void cc_actor_free(cc_node_t *node, cc_actor_t *actor, bool remove_from_registry)
{
	cc_list_t *list = NULL, *tmp_list = NULL;
	cc_calvinsys_obj_t *obj = NULL;

	if (actor == NULL)
		return;

	if (actor->will_end != NULL)
		actor->will_end(actor);

	if (actor->instance_state != NULL && actor->free_state != NULL)
		actor->free_state(actor);

	list = actor->calvinsys->objects;
	while (list != NULL) {
		tmp_list = list;
		list = list->next;
		obj = (cc_calvinsys_obj_t *)tmp_list->data;
		if (obj != NULL && obj->actor != NULL && obj->actor == actor)
			cc_calvinsys_close(actor->calvinsys, tmp_list->id);
	}

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

	if (actor->type != NULL) {
		cc_platform_mem_free((void *)actor->type);
		actor->type = NULL;
	}

	if (actor->id != NULL) {
		cc_log("Actor: Deleting '%s'", actor->id);
		if (remove_from_registry) {
			if (cc_proto_send_remove_actor(node, actor, actor_remove_reply_handler) != CC_SUCCESS)
				cc_log_error("Failed to send command");
		}
		cc_list_remove(&node->actors, actor->id);
		cc_platform_mem_free(actor->id);
		actor->id = NULL;
	}

	if (actor->name != NULL) {
		cc_platform_mem_free(actor->name);
		actor->name = NULL;
	}

	if (actor->private_attributes != NULL)
		cc_actor_free_attribute_list(actor->private_attributes);

	cc_platform_mem_free((void *)actor);
}

cc_actor_t *cc_actor_get(cc_node_t *node, const char *actor_id, uint32_t actor_id_len)
{
	cc_list_t *item = NULL;

	item = cc_list_get_n(node->actors, actor_id, actor_id_len);
	if (item != NULL)
		return (cc_actor_t *)item->data;

	return NULL;
}

void cc_actor_port_state_changed(cc_actor_t *actor)
{
	cc_list_t *list = NULL;
	cc_actor_state_t actor_state = CC_ACTOR_ENABLED;
	cc_port_state_t port_state = CC_PORT_DISCONNECTED;

	list = actor->in_ports;
	while (list != NULL) {
		port_state = ((cc_port_t *)list->data)->state;
		if (port_state == CC_PORT_DO_DELETE) {
			actor_state = CC_ACTOR_DO_DELETE;
			break;
		} else if (port_state != CC_PORT_ENABLED) {
			actor_state = CC_ACTOR_PENDING;
			break;
		}
		list = list->next;
	}

	if (actor_state == CC_ACTOR_ENABLED) {
		list = actor->out_ports;
		while (list != NULL) {
			port_state = ((cc_port_t *)list->data)->state;
			if (port_state == CC_PORT_DO_DELETE) {
				actor_state = CC_ACTOR_DO_DELETE;
				break;
			} else if (port_state != CC_PORT_ENABLED) {
				cc_log("Actor: '%s' disabled", actor->id);
				actor_state = CC_ACTOR_PENDING;
				break;
			}
			list = list->next;
		}
	}

	cc_actor_set_state(actor, actor_state);
}

void cc_actor_port_disconnected(cc_actor_t *actor)
{
	cc_actor_set_state(actor, CC_ACTOR_PENDING);
}

void cc_actor_disconnect(cc_node_t *node, cc_actor_t *actor, bool unref_tunnel)
{
	cc_list_t *list = NULL;

	list = actor->in_ports;
	while (list != NULL) {
		cc_port_disconnect(node, (cc_port_t *)list->data, unref_tunnel);
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		cc_port_disconnect(node, (cc_port_t *)list->data, unref_tunnel);
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
		cc_port_disconnect(node, (cc_port_t *)list->data, true);
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		cc_port_disconnect(node, (cc_port_t *)list->data, true);
		list = list->next;
	}

	return cc_proto_send_actor_new(node, actor, to_rt_uuid, to_rt_uuid_len, cc_actor_migrate_reply_handler);
}

char *cc_actor_serialize(const cc_node_t *node, cc_actor_t *actor, char *buffer, bool include_state)
{
	cc_list_t *in_ports = NULL, *out_ports = NULL;
	cc_list_t *managed_attributes = NULL, *tmp_list = NULL;
	cc_port_t *port = NULL;
	unsigned int nbr_state_attributes = 3;
	unsigned int nbr_inports = 0, nbr_outports = 0, nbr_managed_attributes = 0;
	unsigned int nbr_private_attributes = 0;

	if (actor->private_attributes == NULL) {
		cc_log_error("No private_attributes");
		return NULL;
	}

	if (actor->get_managed_attributes != NULL) {
		if (actor->get_managed_attributes((cc_actor_t *)actor, &managed_attributes) != CC_SUCCESS) {
			cc_log_error("Failed to get managed attributes");
			return NULL;
		}
		nbr_managed_attributes = cc_list_count(managed_attributes);
	}

	if (cc_calvinsys_get_attributes(node->calvinsys, actor, &actor->private_attributes) != CC_SUCCESS) {
		cc_log_error("Failed to get calvinsys attributes");
		if (managed_attributes != NULL)
			cc_actor_free_attribute_list(managed_attributes);
		return NULL;
	}

	nbr_private_attributes = cc_list_count(actor->private_attributes);
	nbr_private_attributes += 2; // in/outports

	in_ports = actor->in_ports;
	if (in_ports != NULL)
		nbr_inports = cc_list_count(in_ports);
	out_ports = actor->out_ports;
	if (out_ports != NULL)
		nbr_outports = cc_list_count(out_ports);

	if (include_state)
		nbr_state_attributes += 1;

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
				buffer = cc_port_serialize_prev_connections(buffer, port, node);
				in_ports = in_ports->next;
			}

			buffer = cc_coder_encode_kv_map(buffer, "outports", nbr_outports);
			out_ports = actor->out_ports;
			while (out_ports != NULL) {
				port = (cc_port_t *)out_ports->data;
				buffer = cc_port_serialize_prev_connections(buffer, port, node);
				out_ports = out_ports->next;
			}
		}

		buffer = cc_coder_encode_kv_map(buffer, "actor_state", 4);
		{
			buffer = cc_coder_encode_kv_map(buffer, "security", 1);
			{
				// TODO: Set proper value
				buffer = cc_coder_encode_kv_nil(buffer, "_subject_attributes");
			}
			buffer = cc_coder_encode_kv_map(buffer, "custom", 0);
			buffer = cc_coder_encode_kv_map(buffer, "managed", nbr_managed_attributes);
			{
				tmp_list = managed_attributes;
				while (tmp_list != NULL) {
					buffer = cc_coder_encode_kv_value(buffer, tmp_list->id, (char *)tmp_list->data, tmp_list->data_len);
					tmp_list = tmp_list->next;
				}
			}
			buffer = cc_coder_encode_kv_map(buffer, "private", nbr_private_attributes);
			{
				tmp_list = actor->private_attributes;
				while (tmp_list != NULL) {
					buffer = cc_coder_encode_kv_value(buffer, tmp_list->id, (char *)tmp_list->data, tmp_list->data_len);
					tmp_list = tmp_list->next;
				}

				buffer = cc_coder_encode_kv_map(buffer, "inports", nbr_inports);
				{
					in_ports = actor->in_ports;
					while (in_ports != NULL) {
						port = (cc_port_t *)in_ports->data;
						buffer = cc_port_serialize_port(buffer, port, include_state);
						in_ports = in_ports->next;
					}
				}

				buffer = cc_coder_encode_kv_map(buffer, "outports", nbr_outports);
				{
					out_ports = actor->out_ports;
					while (out_ports != NULL) {
						port = (cc_port_t *)out_ports->data;
						buffer = cc_port_serialize_port(buffer, port, include_state);
						out_ports = out_ports->next;
					}
				}
			}
		}
	}

	if (managed_attributes != NULL)
		cc_actor_free_attribute_list(managed_attributes);

	return buffer;
}
