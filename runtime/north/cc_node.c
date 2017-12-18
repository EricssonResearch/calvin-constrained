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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cc_node.h"
#include "cc_actor_store.h"
#include "scheduler/cc_scheduler.h"
#include "cc_proto.h"
#include "cc_transport.h"
#include "coder/cc_coder.h"
#include "runtime/south/platform/cc_platform.h"
#ifdef CC_TLS_ENABLED
#include "crypto/cc_crypto.h"
#endif
#include "calvinsys/common/cc_calvinsys_timer.h"
#include "calvinsys/common/cc_calvinsys_attribute.h"
#if CC_USE_PYTHON
#include "libmpy/cc_mpy_port.h"
#endif

#if CC_USE_STORAGE
static cc_result_t cc_node_get_state(cc_node_t *node)
{
	char *buffer = NULL, *value = NULL, *array_value = NULL;
	uint32_t i = 0, value_len = 0, array_size = 0, state = 0;
	cc_link_t *link = NULL;
	cc_tunnel_t *tunnel = NULL;
	cc_actor_t*actor = NULL;
	size_t size;

	if (cc_platform_file_read(CC_STATE_FILE, &buffer, &size) != CC_SUCCESS)
		return CC_FAIL;

	cc_log("Node: Starting from state");

	if (cc_coder_decode_string_from_map(buffer, "id", &value, &value_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'id'");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}
	strncpy(node->id, value, value_len);
	node->id[value_len] = '\0';

	if (node->attributes == NULL && cc_coder_has_key(buffer, "attributes")) {
		if (cc_coder_decode_string_from_map(buffer, "attributes", &value, &value_len) != CC_SUCCESS) {
			cc_log_error("Failed to decode 'attributes'");
			cc_platform_mem_free(buffer);
			return CC_FAIL;
		}

		if (cc_platform_mem_alloc((void **)&node->attributes, value_len + 1) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			cc_platform_mem_free(buffer);
			return CC_FAIL;
		}

		strncpy(node->attributes, value, value_len);
		node->attributes[value_len] = '\0';
	}

	if (node->proxy_uris == NULL && cc_coder_has_key(buffer, "proxy_uris")) {
		if (cc_coder_get_value_from_map(buffer, "proxy_uris", &array_value) == CC_SUCCESS) {
			array_size = cc_coder_get_size_of_array(array_value);
			for (i = 0; i < array_size; i++) {
				if (cc_coder_decode_string_from_array(array_value, i, &value, &value_len) == CC_SUCCESS) {
					if (cc_list_add_n(&node->proxy_uris, value, value_len, NULL, 0) == NULL) {
						cc_log_error("Failed to add uri");
						cc_platform_mem_free(buffer);
						return CC_FAIL;
					}
				}
			}
		}
	}

	if (cc_coder_has_key(buffer, "state")) {
		if (cc_coder_decode_uint_from_map(buffer, "state", &state) == CC_SUCCESS) {
			if (state == CC_NODE_DO_SLEEP)
				node->state = CC_NODE_STARTED;
			else
				node->state = (cc_node_state_t)state;
		}
	}

	if (cc_coder_has_key(buffer, "links")) {
		if (cc_coder_get_value_from_map(buffer, "links", &array_value) == CC_SUCCESS) {
			array_size = cc_coder_get_size_of_array(array_value);
			for (i = 0; i < array_size; i++) {
				if (cc_coder_get_value_from_array(array_value, i, &value) == CC_SUCCESS) {
					link = cc_link_deserialize(node, value);
					if (link == NULL) {
						cc_platform_mem_free(buffer);
						return CC_FAIL;
					} else {
						if (link->is_proxy) {
							node->proxy_link = link;
							cc_log(" Proxy: %s", link->peer_id);
						}
					}
				}
			}
		}
	}

	if (cc_coder_has_key(buffer, "tunnels")) {
		if (cc_coder_get_value_from_map(buffer, "tunnels", &array_value) == CC_SUCCESS) {
			array_size = cc_coder_get_size_of_array(array_value);
			for (i = 0; i < array_size; i++) {
				if (cc_coder_get_value_from_array(array_value, i, &value) == CC_SUCCESS) {
					tunnel = cc_tunnel_deserialize(node, value);
					if (tunnel == NULL) {
						cc_platform_mem_free(buffer);
						return CC_FAIL;
					} else {
						if (tunnel->type == CC_TUNNEL_TYPE_STORAGE) {
							node->storage_tunnel = tunnel;
							cc_log(" Storage tunnel: %s", node->storage_tunnel->id);
						} else if (tunnel->type == CC_TUNNEL_TYPE_PROXY) {
							node->proxy_tunnel = tunnel;
							cc_log(" Proxy tunnel: %s", node->proxy_tunnel->id);
						}
					}
				}
			}
		}
	}

	if (cc_coder_has_key(buffer, "actors")) {
		if (cc_coder_get_value_from_map(buffer, "actors", &array_value) == CC_SUCCESS) {
			array_size = cc_coder_get_size_of_array(array_value);
			for (i = 0; i < array_size; i++) {
				if (cc_coder_get_value_from_array(array_value, i, &value) == CC_SUCCESS) {
					actor = cc_actor_create(node, value);
					if (actor == NULL) {
						cc_platform_mem_free(buffer);
						return CC_FAIL;
					}
				}
			}
		}
	}

	cc_platform_mem_free(buffer);

	return CC_SUCCESS;
}

void cc_node_set_state(cc_node_t *node, bool include_state)
{
	char *buffer = NULL, *tmp = NULL;
	int nbr_of_attributes = 3, nbr_of_items = 0;
	cc_list_t *item = NULL;
	size_t buffer_size = 500;

	if (include_state) {
		// TODO: Find better way to get buffer size when serializing actors
		buffer_size += cc_list_count(node->actors) * 2000;
		nbr_of_attributes = 7;
	}

	if (cc_platform_mem_alloc((void **)&buffer, buffer_size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return;
	}
	memset(buffer, 0, buffer_size);
	tmp = buffer;

	tmp = cc_coder_encode_map(tmp, nbr_of_attributes);
	{
		tmp = cc_coder_encode_kv_str(tmp, "id", node->id, strlen(node->id));
		tmp = cc_coder_encode_kv_str(tmp, "attributes", node->attributes, strnlen(node->attributes, CC_MAX_ATTRIBUTES_LEN));
		nbr_of_items = cc_list_count(node->proxy_uris);
		tmp = cc_coder_encode_kv_array(tmp, "proxy_uris", nbr_of_items);
		{
			item = node->proxy_uris;
			while (item != NULL) {
				tmp = cc_coder_encode_str(tmp, item->id, strnlen(item->id, CC_MAX_URI_LEN));
				item = item->next;
			}
		}

		if (include_state) {
			tmp = cc_coder_encode_kv_uint(tmp, "state", node->state);

			nbr_of_items = cc_list_count(node->links);
			tmp = cc_coder_encode_kv_array(tmp, "links", nbr_of_items);
			{
				item = node->links;
				while (item != NULL) {
					tmp = cc_link_serialize((cc_link_t *)item->data, tmp);
					item = item->next;
				}
			}

			nbr_of_items = cc_list_count(node->tunnels);
			tmp = cc_coder_encode_kv_array(tmp, "tunnels", nbr_of_items);
			{
				item = node->tunnels;
				while (item != NULL) {
					tmp = cc_tunnel_serialize((cc_tunnel_t *)item->data, tmp);
					item = item->next;
				}
			}

			nbr_of_items = cc_list_count(node->actors);
			tmp = cc_coder_encode_kv_array(tmp, "actors", nbr_of_items);
			{
				item = node->actors;
				while (item != NULL) {
					tmp = cc_coder_encode_map(tmp, 1);
					tmp = cc_actor_serialize(node, (cc_actor_t*)item->data, tmp, true);
					item = item->next;
				}
			}
		}
	}

	if (cc_platform_file_write(CC_STATE_FILE, buffer, tmp - buffer) == CC_SUCCESS)
		cc_log("Node: Serialized state");
	else
		cc_log_error("File to write state");
	cc_platform_mem_free(buffer);
}

static void cc_node_reset(cc_node_t *node)
{
	cc_list_t *tmp_list = NULL;

	while (node->actors != NULL) {
		tmp_list = node->actors;
		node->actors = node->actors->next;
		cc_actor_free(node, (cc_actor_t*)tmp_list->data, true);
	}
	node->actors = NULL;

	while (node->tunnels != NULL) {
		tmp_list = node->tunnels;
		node->tunnels = node->tunnels->next;
		cc_tunnel_free(node, (cc_tunnel_t *)tmp_list->data, false);
	}
	node->tunnels = NULL;
	node->storage_tunnel = NULL;
	node->proxy_tunnel = NULL;

	while (node->links != NULL) {
		tmp_list = node->links;
		node->links = node->links->next;
		cc_link_free(node, (cc_link_t *)tmp_list->data);
	}
	node->links = NULL;
	node->proxy_link = NULL;

	while (node->pending_msgs != NULL) {
		tmp_list = node->pending_msgs;
		node->pending_msgs = node->pending_msgs->next;
		cc_platform_mem_free(tmp_list->data);
		cc_platform_mem_free(tmp_list->id);
		cc_platform_mem_free(tmp_list);
	}
	node->pending_msgs = NULL;

	cc_node_set_state(node, false);
}
#endif

cc_result_t cc_node_add_pending_msg(cc_node_t *node, char *msg_uuid, cc_msg_handler_t handler, void *msg_data)
{
	cc_pending_msg_t *msg = NULL;

	if (cc_platform_mem_alloc((void **)&msg, sizeof(cc_pending_msg_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	msg->handler = handler;
	msg->msg_data = msg_data;

	if (cc_list_add_n(&node->pending_msgs, msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE), (void *)msg, sizeof(cc_pending_msg_t)) == NULL) {
		cc_log_error("Failed to add pending msg");
		cc_platform_mem_free(msg);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

void cc_node_remove_pending_msg(cc_node_t *node, char *msg_uuid)
{
	cc_list_t *item = NULL;

	item = cc_list_get_n(node->pending_msgs, msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
	if (item != NULL) {
		cc_platform_mem_free(item->data);
		cc_list_remove(&node->pending_msgs, msg_uuid);
	}
}

cc_pending_msg_t *cc_node_get_pending_msg(cc_node_t *node, const char *msg_uuid)
{
	cc_list_t *item = NULL;

	item = cc_list_get_n(node->pending_msgs, msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
	if (item != NULL)
		return (cc_pending_msg_t *)item->data;

	return NULL;
}

static cc_result_t cc_node_setup_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	uint32_t status;
	char *value = NULL, *obj_time = NULL, *obj_data = NULL;
	cc_coder_type_t type = CC_CODER_UNDEF;

	if (cc_coder_get_value_from_map(data, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'value'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(value, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'value'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(value, "status", &status) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'status'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(value, "data", &obj_data) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'data'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(obj_data, "time", &obj_time) != CC_SUCCESS) {
		cc_log_error("Failed to get 'time'");
		return CC_FAIL;
	}

	if (status != 200) {
		cc_log_error("Failed to setup node, status '%lu'", (unsigned long)status);
		return CC_FAIL;
	}

	type = cc_coder_type_of(obj_time);
	if (type == CC_CODER_FLOAT) {
		float seconds = 0.0;
		if (cc_coder_decode_float(obj_time, &seconds) != CC_SUCCESS) {
			cc_log_error("Failed to decode 'time'");
			return CC_FAIL;
		}
		node->seconds_since_epoch = (uint32_t)seconds;
		node->time_at_sync = cc_platform_get_time();
	} else if (type == CC_CODER_DOUBLE) {
		double seconds = 0.0;
		if (cc_coder_decode_double(obj_time, &seconds) != CC_SUCCESS) {
			cc_log_error("Failed to decode 'time'");
			return CC_FAIL;
		}
		node->seconds_since_epoch = (uint32_t)seconds;
		node->time_at_sync = cc_platform_get_time();
	} else {
		cc_log_error("Unsupported type '%d'", type);
		return CC_FAIL;
	}

	node->state = CC_NODE_STARTED;
	cc_platform_node_started(node);

	cc_log("Node: Connected to proxy");
	cc_log(" id: %s", node->transport_client->peer_id);
	cc_log(" uri: %s", node->transport_client->uri);
	cc_log(" time: %ld (seconds since epoch)", node->seconds_since_epoch);

	return CC_SUCCESS;
}

#if CC_USE_SLEEP
static cc_result_t cc_node_enter_sleep_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	uint32_t status = 0;
	char *value = NULL;

	if (cc_coder_get_value_from_map(data, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'value'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(value, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'value'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(value, "status", &status) == CC_SUCCESS) {
		if (status == 200)
			node->state = CC_NODE_DO_SLEEP;
		else
			node->state = CC_NODE_STARTED;
	}

	return CC_SUCCESS;
}
#endif

cc_result_t cc_node_handle_token(cc_port_t *port, const char *data, const size_t size, uint32_t sequencenbr)
{
	char *buffer = NULL;

	if (port->actor->state == CC_ACTOR_ENABLED) {
		if (cc_fifo_slots_available(port->fifo, 1)) {
			if (cc_platform_mem_alloc((void **)&buffer, size) != CC_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				return CC_FAIL;
			}
			memcpy(buffer, data, size);
			if (cc_fifo_com_write(port->fifo, buffer, size, sequencenbr) == CC_SUCCESS)
				return CC_SUCCESS;
			cc_log_error("Failed to write to fifo");
			cc_platform_mem_free((void *)buffer);
		} else
			cc_log_debug("Token received but no slots available");
	} else
		cc_log_debug("Token received but actor not enabled");

	return CC_FAIL;
}

void cc_node_handle_token_reply(cc_node_t *node, char *port_id, uint32_t port_id_len, cc_port_reply_type_t reply_type, uint32_t sequencenbr)
{
	cc_port_t *port = cc_port_get(node, port_id, port_id_len);

	if (port != NULL) {
		if (reply_type == CC_PORT_REPLY_TYPE_ACK)
			cc_fifo_com_commit_read(port->fifo, sequencenbr);
		else if (reply_type == CC_PORT_REPLY_TYPE_NACK)
			cc_fifo_com_cancel_read(port->fifo, sequencenbr);
		else if (reply_type == CC_PORT_REPLY_TYPE_ABORT)
			cc_log_debug("TODO: handle ABORT");
	}
}

cc_result_t cc_node_handle_message(cc_node_t *node, char *buffer, size_t len)
{
	if (cc_proto_parse_message(node, buffer, len) == CC_SUCCESS) {
#if (CC_USE_STORAGE == 1 && CC_USE_CHECKPOINTING == 1)
		// message successfully handled == state changed -> serialize the node
		if (node->state == CC_NODE_STARTED)
			cc_node_set_state(node, true);
#endif
		return CC_SUCCESS;
	}

	cc_log_error("Failed to handle message");
	return CC_FAIL;
}

static cc_result_t cc_node_setup(cc_node_t *node)
{
#ifdef CC_TLS_ENABLED
	char name[50];
	char domain[50];

	return crypto_get_node_info(domain, name, node->id);
#endif

#if CC_USE_STORAGE
	if (cc_platform_file_stat(CC_STATE_FILE) == CC_STAT_FILE) {
		if (cc_node_get_state(node) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_log("Node: Failed to get state, resetting node");
		cc_node_reset(node);
	}
#endif

	cc_gen_uuid(node->id, NULL);

	return CC_SUCCESS;
}

static cc_result_t cc_node_connect_to_proxy(cc_node_t *node, char *uri)
{
	char *peer_id = NULL;
	size_t peer_id_len = 0;
	cc_list_t *tmp_list = NULL;
	bool new_proxy = true;

	if (node->transport_client == NULL) {
		node->transport_client = cc_transport_create(node, uri);
		if (node->transport_client == NULL)
			return CC_FAIL;
	}

	while (node->state != CC_NODE_STOP && node->transport_client->state == CC_TRANSPORT_INTERFACE_DOWN) {
		if (cc_platform_evt_wait(node, CC_INDEFINITELY_TIMEOUT) == CC_PLATFORM_EVT_WAIT_FAIL)
			return CC_FAIL;
	}

	if (node->transport_client->connect(node, node->transport_client) != CC_SUCCESS)
		return CC_FAIL;

	while (node->state != CC_NODE_STOP && node->transport_client->state == CC_TRANSPORT_PENDING) {
		if (cc_platform_evt_wait(node, CC_INDEFINITELY_TIMEOUT) == CC_PLATFORM_EVT_WAIT_FAIL)
			return CC_FAIL;
	}

	if (cc_transport_join(node, node->transport_client) != CC_SUCCESS)
		return CC_FAIL;

	while (node->state != CC_NODE_STOP && node->transport_client->state == CC_TRANSPORT_PENDING) {
		if (cc_platform_evt_wait(node, CC_INDEFINITELY_TIMEOUT) == CC_PLATFORM_EVT_WAIT_FAIL)
			return CC_FAIL;
	}

	if (node->state == CC_NODE_STOP || node->transport_client->state != CC_TRANSPORT_ENABLED)
		return CC_FAIL;

	peer_id = node->transport_client->peer_id;
	peer_id_len = strnlen(peer_id, CC_UUID_BUFFER_SIZE);

	if (node->proxy_link != NULL) {
		if (strncmp(node->proxy_link->peer_id, peer_id, peer_id_len) == 0)
			new_proxy = false;
		else {
			cc_log("New proxy");
			while (node->tunnels != NULL) {
				tmp_list = node->tunnels;
				node->tunnels = node->tunnels->next;
				cc_tunnel_free(node, (cc_tunnel_t *)tmp_list->data, false);
			}
			node->tunnels = NULL;
			node->storage_tunnel = NULL;
			node->proxy_tunnel = NULL;

			while (node->links != NULL) {
				tmp_list = node->links;
				node->links = node->links->next;
				cc_link_free(node, (cc_link_t *)tmp_list->data);
			}
			node->links = NULL;
			node->proxy_link = NULL;

			tmp_list = node->actors;
			while (tmp_list != NULL) {
				cc_actor_disconnect(node, (cc_actor_t *)tmp_list->data, false);
				tmp_list = tmp_list->next;
			}
		}
	}

	if (node->proxy_link == NULL) {
		node->proxy_link = cc_link_create(node, peer_id, peer_id_len, true);
		if (node->proxy_link == NULL) {
			cc_log_error("Failed to create proxy link");
			return CC_FAIL;
		}
	}

	if (node->storage_tunnel == NULL) {
		node->storage_tunnel = cc_tunnel_create(node, CC_TUNNEL_TYPE_STORAGE, CC_TUNNEL_DISCONNECTED, peer_id, peer_id_len, NULL, 0);
		if (node->storage_tunnel == NULL) {
			cc_log_error("Failed to create storage tunnel");
			return CC_FAIL;
		}
		cc_tunnel_add_ref(node->storage_tunnel);
	}

	if (node->proxy_tunnel == NULL) {
		node->proxy_tunnel = cc_tunnel_create(node, CC_TUNNEL_TYPE_PROXY, CC_TUNNEL_DISCONNECTED, peer_id, peer_id_len, NULL, 0);
		if (node->proxy_tunnel == NULL) {
			cc_log_error("Failed to create proxy tunnel");
			return CC_FAIL;
		}
		cc_tunnel_add_ref(node->proxy_tunnel);
	}

	while ((node->storage_tunnel->state == CC_TUNNEL_PENDING || node->proxy_tunnel->state == CC_TUNNEL_PENDING)
		&& node->state != CC_NODE_STOP && node->transport_client->state == CC_TRANSPORT_ENABLED) {
		if (cc_platform_evt_wait(node, CC_INDEFINITELY_TIMEOUT) == CC_PLATFORM_EVT_WAIT_FAIL)
			return CC_FAIL;
	}

	if (node->storage_tunnel->state != CC_TUNNEL_ENABLED) {
		cc_log_error("Failed to connect storage tunnel");
		return CC_FAIL;
	}

	if (node->proxy_tunnel->state != CC_TUNNEL_ENABLED) {
		cc_log_error("Failed to connect proxy tunnel");
		return CC_FAIL;
	}

	if (new_proxy) {
		if (cc_proto_send_node_setup(node, cc_node_setup_reply_handler) != CC_SUCCESS)
			return CC_FAIL;
	} else {
		if (cc_proto_send_wake_signal(node, cc_node_setup_reply_handler) != CC_SUCCESS)
			return CC_FAIL;
	}

	while (node->state != CC_NODE_STARTED && node->state != CC_NODE_STOP && node->transport_client->state == CC_TRANSPORT_ENABLED) {
		if (cc_platform_evt_wait(node, CC_INDEFINITELY_TIMEOUT) == CC_PLATFORM_EVT_WAIT_FAIL)
			return CC_FAIL;
	}

	if (node->state != CC_NODE_STARTED) {
		cc_log_error("Failed connect to proxy");
		return CC_FAIL;
	}

	tmp_list = node->actors;
	while (tmp_list != NULL) {
		cc_actor_connect_ports(node, (cc_actor_t *)tmp_list->data);
		tmp_list = tmp_list->next;
	}

	return CC_SUCCESS;
}

cc_result_t cc_node_init(cc_node_t *node, const char *attributes, const char *proxy_uris, const char *storage_dir)
{
	char *uris = (char *)proxy_uris, *uri = NULL;
	cc_list_t *item = NULL;
	cc_actor_t *actor = NULL;
	cc_calvinsys_capability_t capabilities[] = {
		CC_CAPABILITIES
	};
	cc_actor_builtin_type_t actor_types[] = {
		CC_C_ACTORS
	};

	node->state = CC_NODE_DO_START;
	node->stop_method = CC_NODE_STOP_CLEAN;
	node->fire_actors = fire_actors;
	node->transport_client = NULL;
	node->proxy_link = NULL;
	node->platform = NULL;
	node->links = NULL;
	node->storage_tunnel = NULL;
	node->proxy_tunnel = NULL;
	node->tunnels = NULL;
	node->actors = NULL;
	node->seconds_since_epoch = 0;
	node->time_at_sync = 0;

	if (attributes != NULL) {
		if (strlen(attributes) <= CC_MAX_ATTRIBUTES_LEN) {
			if (cc_platform_mem_alloc((void **)&node->attributes, strnlen(attributes, CC_MAX_ATTRIBUTES_LEN) + 1) != CC_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				return CC_FAIL;
			}
			strcpy(node->attributes, attributes);
		} else {
			cc_log_error("Attributes to big");
			return CC_FAIL;
		}
	}

	if (storage_dir != NULL) {
		if (strlen(storage_dir) <= CC_MAX_DIR_PATH) {
			if (cc_platform_mem_alloc((void **)&node->storage_dir, strnlen(storage_dir, CC_MAX_DIR_PATH) + 1) != CC_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				return CC_FAIL;
			}
			strcpy(node->storage_dir, storage_dir);
		}
	}

	if (cc_platform_create(node) != CC_SUCCESS) {
		cc_log_error("Failed to create platform object");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&node->calvinsys, sizeof(cc_calvinsys_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	node->calvinsys->node = node;
	node->calvinsys->capabilities = NULL;
	node->calvinsys->objects = NULL;
	node->actor_types = NULL;

	if (cc_calvinsys_timer_create(&node->calvinsys) != CC_SUCCESS) {
		cc_log_error("Failed to create calvinsys 'timer'");
		return CC_FAIL;
	}

	if (cc_calvinsys_attribute_create(&node->calvinsys) != CC_SUCCESS) {
		cc_log_error("Failed to create calvinsys 'attribute'");
		return CC_FAIL;
	}

	if (cc_calvinsys_add_capabilities(node->calvinsys, sizeof(capabilities) / sizeof(cc_calvinsys_capability_t), capabilities) != CC_SUCCESS) {
		cc_log_error("Failed to add capabilities");
		return CC_FAIL;
	}

	if (cc_actor_store_init(&node->actor_types, sizeof(actor_types) / sizeof(cc_actor_builtin_type_t), actor_types) != CC_SUCCESS) {
		cc_log_error("Failed to init C actors");
		return CC_FAIL;
	}

	if (cc_node_setup(node) != CC_SUCCESS) {
		cc_log_error("Failed to setup runtime");
		return CC_FAIL;
	}

	if (node->proxy_uris == NULL && uris != NULL) {
		uri = strtok(uris, " ");
		while (uri != NULL) {
			if (strlen(uri) <= CC_MAX_URI_LEN) {
				if (cc_list_add_n(&node->proxy_uris, uri, strlen(uri), NULL, 0) == NULL) {
					cc_log_error("Failed to add URI");
					return CC_FAIL;
				}
			} else
				cc_log_error("URI to big");
			uri = strtok(NULL, " ");
		}
	}

	if (cc_list_count(node->proxy_uris) == 0) {
		cc_log_error("No proxy(s) set");
		return CC_FAIL;
	}


	cc_log("----------------------------------------");
	cc_log("ID: %s", node->id);
	if (node->attributes != NULL)
		cc_log("Attributes: %s", node->attributes);
	if (node->proxy_uris != NULL) {
		cc_log("Proxy URIs:");
		item = node->proxy_uris;
		while (item != NULL) {
			cc_log(" %s", item->id);
			item = item->next;
		}
	}
	if (node->calvinsys != NULL) {
		cc_log("Capabilities:");
		item = node->calvinsys->capabilities;
		while (item != NULL) {
			cc_log(" %s", item->id);
			item = item->next;
		}
	}
	if (node->actor_types != NULL) {
		cc_log("C actor types:");
		item = node->actor_types;
		while (item != NULL) {
			cc_log(" %s", item->id);
			item = item->next;
		}
	}
	if (node->storage_dir != NULL)
		cc_log("Storage directory: %s", node->storage_dir);
	cc_log("Inactivity timeout: %d seconds", CC_INACTIVITY_TIMEOUT);
#if CC_USE_SLEEP
	cc_log("Default sleep time: %d seconds", CC_SLEEP_TIME);
#endif
	if (node->actors != NULL) {
		cc_log("Actors:");
		item = node->actors;
		while (item != NULL) {
			actor = (cc_actor_t*)item->data;
			cc_log(" %s %s", actor->id, actor->name);
			item = item->next;
		}
	}
	cc_log("----------------------------------------");

	return CC_SUCCESS;
}

static void cc_node_free(cc_node_t *node, bool cleanup)
{
	cc_list_t *item = NULL, *tmp_item = NULL;

	item = node->proxy_uris;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		cc_platform_mem_free((void *)tmp_item->id);
		cc_platform_mem_free((void *)tmp_item);
	}

	item = node->actor_types;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		cc_platform_mem_free((void *)tmp_item->id);
		cc_platform_mem_free((void *)tmp_item->data);
		cc_platform_mem_free((void *)tmp_item);
	}

	item = node->actors;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		cc_actor_free(node, (cc_actor_t*)tmp_item->data, cleanup);
	}

	item = node->tunnels;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		cc_tunnel_free(node, (cc_tunnel_t *)tmp_item->data, false);
	}

	item = node->links;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		cc_link_free(node, (cc_link_t *)tmp_item->data);
	}

	if (node->platform != NULL)
		cc_platform_mem_free((void *)node->platform);

	item = node->calvinsys->capabilities;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		cc_calvinsys_delete_capability(node->calvinsys, tmp_item->id);
	}
	cc_platform_mem_free((void *)node->calvinsys);

	if (node->attributes != NULL)
		cc_platform_mem_free((void *)node->attributes);

	if (node->storage_dir != NULL)
		cc_platform_mem_free((void *)node->storage_dir);

	while (node->pending_msgs != NULL) {
		item = node->pending_msgs;
		node->pending_msgs = node->pending_msgs->next;
		cc_platform_mem_free(item->data);
		cc_platform_mem_free(item->id);
		cc_platform_mem_free(item);
	}

#if CC_USE_PYTHON
	cc_mpy_port_deinit();
	cc_platform_mem_free(node->mpy_heap);
#endif

	cc_platform_mem_free((void *)node);
}

uint32_t cc_node_get_time(cc_node_t *node)
{
	// TODO: Handle wraps
	return node->seconds_since_epoch + (cc_platform_get_time() - node->time_at_sync);
}

#if CC_USE_SLEEP
static void cc_node_enter_sleep(cc_node_t *node, uint32_t seconds_to_sleep)
{
	if (cc_proto_send_sleep_request(node, seconds_to_sleep, cc_node_enter_sleep_reply_handler) != CC_SUCCESS) {
		cc_log_error("Failed to send sleep request");
		node->state = CC_NODE_STARTED;
		return;
	}

	cc_log("Node: Sleep requested for %ld seconds", seconds_to_sleep);

	node->state = CC_NODE_PENDING;
	cc_platform_evt_wait(node, CC_INDEFINITELY_TIMEOUT);
	if (node->state != CC_NODE_DO_SLEEP) {
		cc_log("Sleep request denied");
		node->state = CC_NODE_STARTED;
		return;
	}

	cc_log("Node: Enterring sleep for %ld seconds", seconds_to_sleep);

#if CC_USE_STORAGE
	cc_node_set_state(node, true);
#else
	cc_log("Going to sleep without serializing node state");
#endif

	if (node->transport_client != NULL) {
		node->transport_client->disconnect(node, node->transport_client);
		node->transport_client->free(node->transport_client);
	}

	cc_platform_stop(node);
	cc_node_free(node, false);
	cc_platform_deepsleep(seconds_to_sleep);
}
#endif

void cc_node_stop(cc_node_t *node)
{
	cc_list_t *item = NULL, *tmp_item = NULL;

	cc_log("Node: Stopping");

	item = node->actors;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		if (node->stop_method == CC_NODE_STOP_MIGRATE) {
			if (cc_actor_migrate(node, (cc_actor_t *)tmp_item->data, node->proxy_link->peer_id, strlen(node->proxy_link->peer_id)) != CC_SUCCESS)
				cc_log_error("Failed to migrate '%s'", ((cc_actor_t *)tmp_item->data)->id);
			cc_actor_free(node, (cc_actor_t *)tmp_item->data, false);
		} else
			cc_actor_free(node, (cc_actor_t *)tmp_item->data, true);
	}

	if (node->proxy_tunnel != NULL) {
		cc_tunnel_remove_ref(node, node->proxy_tunnel);
		node->proxy_tunnel = NULL;
	}

	if (node->storage_tunnel != NULL) {
		cc_tunnel_remove_ref(node, node->storage_tunnel);
		node->storage_tunnel = NULL;
	}
}

cc_result_t cc_node_run(cc_node_t *node)
{
	cc_list_t *item = NULL;
	uint32_t timeout = 0, timer_timeout = 0;
#if CC_USE_SLEEP
	uint8_t connect_failures = 0;
	uint32_t seconds_to_sleep = 0;
#endif
	cc_platform_evt_wait_status_t waitstatus = CC_PLATFORM_EVT_WAIT_DATA_READ;

	if (node->fire_actors == NULL) {
		cc_log_error("No actor scheduler set");
		return CC_FAIL;
	}

	while (node->state != CC_NODE_STOP) {
		item = node->proxy_uris;
		while (item != NULL && node->state != CC_NODE_STOP) {
			node->state = CC_NODE_DO_START;
			cc_log("Node: Connecting to proxy with '%s'", item->id);
			if (cc_node_connect_to_proxy(node, item->id) == CC_SUCCESS) {
#if CC_USE_SLEEP
				connect_failures = 0;
#endif
				while (node->state != CC_NODE_STOP && node->transport_client->state == CC_TRANSPORT_ENABLED) {
					if (node->state != CC_NODE_STARTED || node->actors == NULL) {
						waitstatus = cc_platform_evt_wait(node, CC_INDEFINITELY_TIMEOUT);
						if (waitstatus == CC_PLATFORM_EVT_WAIT_FAIL)
							break;
						else
							continue;
					}

					if (node->fire_actors(node))
						continue;

					timeout = CC_INACTIVITY_TIMEOUT;
					if (cc_calvinsys_timer_get_nexttrigger(node, &timer_timeout) == CC_SUCCESS) {
						if (timer_timeout < CC_INACTIVITY_TIMEOUT) {
							// timer active and < CC_INACTIVITY_TIMEOUT, use as timeout
							timeout = timer_timeout;
						}
					}

					waitstatus = cc_platform_evt_wait(node, timeout);
					if (waitstatus == CC_PLATFORM_EVT_WAIT_FAIL)
						break;
					else if (waitstatus == CC_PLATFORM_EVT_WAIT_DATA_READ)
						continue;

					cc_log_debug("Idle for '%ld' seconds", timeout);

#if CC_USE_SLEEP
					// idle, enter sleep
					seconds_to_sleep = CC_SLEEP_TIME;
					if (cc_calvinsys_timer_get_nexttrigger(node, &seconds_to_sleep) == CC_SUCCESS) {
						if (seconds_to_sleep < CC_INACTIVITY_TIMEOUT) {
							// timer about to trigger, continue exection
							continue;
						}
					}
					cc_node_enter_sleep(node, seconds_to_sleep);
#endif
				}

				if (node->state == CC_NODE_STOP)
					cc_node_stop(node);
			} else {
#if CC_USE_SLEEP
				connect_failures++;
#endif
			}

			if (node->transport_client != NULL) {
				cc_transport_disconnect(node, node->transport_client);
				node->transport_client->free(node->transport_client);
				node->transport_client = NULL;
			}
			item = item->next;
		}

		if (node->state != CC_NODE_STOP) {
#if CC_USE_SLEEP
			if (connect_failures >= 5) {
				cc_log("Node: No proxy found, enterring sleep");
				cc_node_set_state(node, true);
				cc_platform_deepsleep(CC_SLEEP_TIME);
			}
#endif
			cc_log("Node: No proxy found, waiting '%d' seconds", CC_RECONNECT_TIMEOUT);
			cc_platform_evt_wait(node, CC_RECONNECT_TIMEOUT);
		}
	}

#if CC_USE_STORAGE
	cc_node_set_state(node, false);
#endif
	cc_platform_stop(node);
	cc_node_free(node, false);
	cc_log("Node: Stopped");

	return CC_SUCCESS;
}
