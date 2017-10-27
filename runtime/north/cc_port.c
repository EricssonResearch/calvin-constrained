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
#include <stdio.h>
#include "cc_port.h"
#include "cc_node.h"
#include "cc_proto.h"
#include "coder/cc_coder.h"
#include "runtime/south/platform/cc_platform.h"

static cc_port_t *cc_port_get_from_peer_port_id(struct cc_node_t *node, const char *peer_port_id, uint32_t peer_port_id_len);
static void cc_port_set_state(cc_port_t *port, cc_port_state_t state);
static cc_result_t cc_port_get_tunnel(cc_node_t *node, cc_port_t *port);
static cc_result_t cc_port_connect_local(cc_node_t *node, cc_port_t *port);

static cc_result_t cc_port_remove_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	// No action
	return CC_SUCCESS;
}

static cc_result_t cc_port_disconnect_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	// No action
	return CC_SUCCESS;
}

static cc_result_t cc_port_store_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	char *value = NULL, *response = NULL, *key = NULL;
	uint32_t key_len = 0, status = 0;
	cc_port_t *port = NULL;

	if (cc_coder_get_value_from_map(data, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to get 'value'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(value, "key", &key, &key_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'key'");
		return CC_FAIL;
	}

	if (strncmp(key, "port-", 5) != 0) {
		cc_log_error("Unexpected key");
		return CC_FAIL;
	}

	port = cc_port_get(node, key + 5, key_len - 5);
	if (port == NULL)
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
		cc_log_debug("Stored '%s'", port->id);
	else
		cc_log_error("Failed to store '%s'", port->id);

	return CC_SUCCESS;
}

static cc_result_t cc_port_get_peer_port_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	cc_port_t *port = NULL;
	char *value = NULL, *value_value = NULL, *key = NULL, *node_id = NULL;
	uint32_t key_len = 0, node_id_len = 0;

	if (cc_coder_get_value_from_map(data, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to get 'value'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(value, "key", &key, &key_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'key'");
		return CC_FAIL;
	}

	port = cc_port_get_from_peer_port_id(node, key + 5, key_len - 5);
	if (port == NULL) {
		cc_log_debug("Unknown port");
		return CC_SUCCESS;
	}

	if (port->state == CC_PORT_ENABLED) {
		cc_log_debug("Port already connected");
		return CC_SUCCESS;
	}

	if (cc_coder_get_value_from_map(value, "value", &value_value) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'value'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(value_value, "node_id", &node_id, &node_id_len) != CC_SUCCESS) {
		cc_log_debug("Failed to decode 'node_id'");
		cc_port_set_state(port, CC_PORT_DO_DELETE);
		return CC_FAIL;
	}

	strncpy(port->peer_id, node_id, node_id_len);
	port->peer_id[node_id_len] = '\0';

	cc_log_debug("Got peer port respose, connecting '%s'", port->id);
	cc_port_connect(node, port);

	return CC_SUCCESS;
}

static cc_result_t cc_port_connect_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	char *value = NULL, *value_data = NULL, *peer_port_id = NULL;
	uint32_t status = 0, peer_port_id_len = 0;
	cc_port_t *port = NULL;

	if (cc_coder_get_value_from_map(data, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to get 'value'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(value, "status", &status) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'status'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(value, "data", &value_data) != CC_SUCCESS) {
		cc_log_error("Failed to get 'data'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(value_data, "port_id", &peer_port_id, &peer_port_id_len) == CC_SUCCESS)
		port = cc_port_get_from_peer_port_id(node, peer_port_id, peer_port_id_len);

	if (port != NULL) {
		if (status == 200)
			cc_port_set_state(port, CC_PORT_ENABLED);
		else
			cc_port_set_state(port, CC_PORT_DISCONNECTED);
	}

	return CC_SUCCESS;
}

void cc_port_set_state(cc_port_t *port, cc_port_state_t state)
{
	port->state = state;
	if (state == CC_PORT_ENABLED && port->state != CC_PORT_ENABLED) {
		cc_log("Port: Enabled '%s'", port->id);
		port->retries = 0;
	} else if (state == CC_PORT_DISCONNECTED && port->state != CC_PORT_DISCONNECTED)
		cc_log("Port: Disconnected '%s'", port->id);
	cc_actor_port_state_changed(port->actor);
}

cc_port_t *cc_port_create(cc_node_t *node, cc_actor_t *actor, char *obj_port, char *obj_prev_connections, cc_port_direction_t direction)
{
	char *obj_prev_ports = NULL, *obj_prev_port = NULL, *obj_peer = NULL, *obj_queue = NULL, *obj_properties = NULL;
	char *r = obj_port, *port_id = NULL, *port_name = NULL, *routing = NULL, *peer_id = NULL, *peer_port_id = NULL, *tmp_value = NULL;
	uint32_t nbr_peers = 0, port_id_len = 0, port_name_len = 0, routing_len = 0, peer_id_len = 0, peer_port_id_len = 0;
	cc_port_t *port = NULL;

	if (cc_coder_decode_string_from_map(r, "id", &port_id, &port_id_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'id'");
		return NULL;
	}

	if (cc_coder_decode_string_from_map(r, "name", &port_name, &port_name_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'name'");
		return NULL;
	}

	if (cc_coder_get_value_from_map(r, "properties", &obj_properties) != CC_SUCCESS) {
		cc_log_error("Failed to get 'properties'");
		return NULL;
	}

	if (cc_coder_decode_string_from_map(obj_properties, "routing", &routing, &routing_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'routing'");
		return NULL;
	}

	if (strncmp("default", routing, routing_len) != 0 && strncmp("fanout", routing, routing_len) != 0) {
		cc_log_error("Unsupported routing");
		return NULL;
	}

	if (cc_coder_decode_uint_from_map(obj_properties, "nbr_peers", &nbr_peers) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'nbr_peers'");
		return NULL;
	}

	if (nbr_peers != 1) {
		cc_log_error("Fanout/fanin not supported (TODO: Implement)");
		return NULL;
	}

	if (cc_coder_get_value_from_map(r, "queue", &obj_queue) != CC_SUCCESS) {
		cc_log_error("Failed to get 'queue'");
		return NULL;
	}

	if (direction == CC_PORT_DIRECTION_IN) {
		if (cc_coder_get_value_from_map(obj_prev_connections, "inports", &obj_prev_ports) != CC_SUCCESS) {
			cc_log_error("Failed to get 'inports'");
			return NULL;
		}
	} else {
		if (cc_coder_get_value_from_map(obj_prev_connections, "outports", &obj_prev_ports) != CC_SUCCESS) {
			cc_log_error("Failed to get 'outports'");
			return NULL;
		}
	}

	if (cc_coder_get_value_from_map_n(obj_prev_ports, port_id, port_id_len, &obj_prev_port) != CC_SUCCESS) {
		cc_log_error("Failed to get value");
		return NULL;
	}

	if (cc_coder_get_value_from_array(obj_prev_port, 0, &obj_peer) != CC_SUCCESS) {
		cc_log_error("Failed to get value");
		return NULL;
	}

	if (cc_coder_get_value_from_array(obj_peer, 0, &tmp_value) != CC_SUCCESS) {
		cc_log_error("Failed to get value");
		return NULL;
	}

	if (cc_coder_type_of(tmp_value) == CC_CODER_STR) {
		if (cc_coder_decode_str(tmp_value, &peer_id, &peer_id_len) != CC_SUCCESS) {
			cc_log_error("Failed to decode peer_id");
			return NULL;
		}
	}

	if (cc_coder_decode_string_from_array(obj_peer, 1, &peer_port_id, &peer_port_id_len) != CC_SUCCESS) {
		cc_log_error("Failed to get peer_port_id");
		return NULL;
	}

	if (cc_platform_mem_alloc((void **)&port, sizeof(cc_port_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	memset(port, 0, sizeof(cc_port_t));
	port->retries = 0;
	port->direction = direction;
	port->state = CC_PORT_DISCONNECTED;
	port->actor = actor;
	port->peer_port = NULL;
	port->tunnel = NULL;
	strncpy(port->id, port_id, port_id_len);
	port->id[port_id_len] = '\0';
	strncpy(port->name, port_name, port_name_len);
	port->name[port_name_len] = '\0';
	strncpy(port->peer_port_id, peer_port_id, peer_port_id_len);
	port->peer_port_id[peer_port_id_len] = '\0';
	if (peer_id != NULL) {
		strncpy(port->peer_id, peer_id, peer_id_len);
		port->peer_id[peer_id_len] = '\0';
	}

	port->fifo = cc_fifo_init(obj_queue);
	if (port->fifo ==  NULL) {
		cc_log_error("Failed to init fifo");
		cc_port_free(node, port, false);
		return NULL;
	}

	if (cc_coder_has_key(r, "constrained_state")) {
		if (cc_coder_decode_uint_from_map(r, "constrained_state", (uint32_t *)&port->state) != CC_SUCCESS) {
			cc_log_error("Failed to decode 'constrained_state'");
			cc_port_free(node, port, false);
			return NULL;
		}
		if (port->state == CC_PORT_ENABLED) {
			if (strncmp(port->peer_id, node->id, strnlen(port->peer_id, CC_UUID_BUFFER_SIZE)) == 0)
				cc_port_connect_local(node, port);
			else {
				port->tunnel = cc_tunnel_get_from_peerid_and_type(node, port->peer_id, peer_id_len, CC_TUNNEL_TYPE_TOKEN);
				if (port->tunnel != NULL)
					cc_tunnel_add_ref(port->tunnel);
				else
					cc_port_set_state(port, CC_PORT_DISCONNECTED);
			}
		}
	} else {
		if (node->transport_client != NULL) {
			if (cc_proto_send_set_port(node, port, cc_port_store_reply_handler) != CC_SUCCESS) {
				cc_log_error("Failed to store port");
				cc_port_free(node, port, false);
				return NULL;
			}
		}
	}

	cc_log("Port: Created '%s'", port->id);

	return port;
}

void cc_port_free(cc_node_t *node, cc_port_t *port, bool remove_from_registry)
{
	cc_log("Port: Deleting '%s'", port->id);

	if (remove_from_registry) {
		if (cc_proto_send_remove_port(node, port, cc_port_remove_reply_handler) != CC_SUCCESS)
			cc_log_error("Failed to remove port '%s'", port->id);
	}

	if (port->tunnel != NULL)
		cc_tunnel_remove_ref(node, port->tunnel);
	cc_fifo_free(port->fifo);
	cc_platform_mem_free((void *)port);
}

cc_port_t *cc_port_get(cc_node_t *node, const char *port_id, uint32_t port_id_len)
{
	cc_list_t *port = NULL, *actors = node->actors;
	cc_actor_t *actor = NULL;

	while (actors != NULL) {
		actor = (cc_actor_t *)actors->data;

		port = cc_list_get_n(actor->in_ports, port_id, port_id_len);
		if (port != NULL)
			return (cc_port_t *)port->data;

		port = cc_list_get_n(actor->out_ports, port_id, port_id_len);
		if (port != NULL)
			return (cc_port_t *)port->data;

		actors = actors->next;
	}

	return NULL;
}

cc_port_t *cc_port_get_from_peer_port_id(struct cc_node_t *node, const char *peer_port_id, uint32_t peer_port_id_len)
{
	cc_list_t *actors = node->actors, *ports = NULL;
	cc_actor_t *actor = NULL;
	cc_port_t *port = NULL;

	while (actors != NULL) {
		actor = (cc_actor_t *)actors->data;

		ports = actor->in_ports;
		while (ports != NULL) {
			port = (cc_port_t *)ports->data;
			if (strncmp(port->peer_port_id, peer_port_id, peer_port_id_len) == 0)
				return port;
			ports = ports->next;
		}

		ports = actor->out_ports;
		while (ports != NULL) {
			port = (cc_port_t *)ports->data;
			if (strncmp(port->peer_port_id, peer_port_id, peer_port_id_len) == 0)
				return port;
			ports = ports->next;
		}
		actors = actors->next;
	}

	return NULL;
}

cc_port_t *cc_port_get_from_name(cc_actor_t *actor, const char *name, size_t name_len, cc_port_direction_t direction)
{
	cc_list_t *ports = NULL;
	cc_port_t *port = NULL;

	if (direction == CC_PORT_DIRECTION_IN)
		ports = actor->in_ports;
	else
		ports = actor->out_ports;

	while (ports != NULL) {
		port = (cc_port_t *)ports->data;
		if (port->direction == direction && strncmp(port->name, name, name_len) == 0)
			return port;
		ports = ports->next;
	}

	return NULL;
}

cc_result_t cc_port_handle_connect(cc_node_t *node, const char *port_id, uint32_t port_id_len, const char *tunnel_id, uint32_t tunnel_id_len)
{
	cc_port_t *port = NULL;

	port = cc_port_get(node, port_id, port_id_len);
	if (port == NULL) {
		cc_log_error("Failed to get port");
		return CC_FAIL;
	}

	if (port->tunnel != NULL) {
		if (strncmp(port->tunnel->id, tunnel_id, tunnel_id_len) == 0) {
			cc_port_set_state(port, CC_PORT_ENABLED);
			return CC_SUCCESS;
		}
		cc_tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
	}

	port->tunnel = cc_tunnel_get_from_id(node, tunnel_id, tunnel_id_len);
	if (port->tunnel == NULL) {
		cc_log_error("Failed to get tunnel");
		return CC_FAIL;
	}

	cc_tunnel_add_ref(port->tunnel);
	cc_port_set_state(port, CC_PORT_ENABLED);
	memset(port->peer_id, 0, CC_UUID_BUFFER_SIZE);
	strncpy(port->peer_id, port->tunnel->link->peer_id, strnlen(port->tunnel->link->peer_id, CC_UUID_BUFFER_SIZE));
	cc_log_debug("'%s' connected by remote on tunnel '%s'", port->id, port->tunnel->id);

	return CC_SUCCESS;
}

cc_result_t cc_port_handle_disconnect(cc_node_t *node, const char *port_id, uint32_t port_id_len)
{
	cc_port_t *port = NULL;

	port = cc_port_get(node, port_id, port_id_len);
	if (port == NULL) {
		cc_log_error("Failed to get port");
		return CC_FAIL;
	}

	cc_fifo_cancel(port->fifo);
	cc_port_set_state(port, CC_PORT_DISCONNECTED);
	memset(port->peer_id, 0, CC_UUID_BUFFER_SIZE);

	if (port->tunnel != NULL) {
		cc_tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
	}

	cc_log_debug("'%s' disconnected by remote", port->id);

	return CC_SUCCESS;
}

static void cc_port_do_peer_lookup(cc_node_t *node, cc_port_t *port)
{
	cc_log_debug("Getting peer port '%s' for '%s'", port->peer_port_id, port->id);
	if (cc_proto_send_get_port(node, port->peer_port_id, cc_port_get_peer_port_reply_handler, port->id) != CC_SUCCESS) {
		cc_port_set_state(port, CC_PORT_DISCONNECTED);
		cc_log_error("Failed to send get port");
	} else
		cc_port_set_state(port, CC_PORT_PENDING_LOOKUP);
}

static cc_result_t cc_port_connect_local(cc_node_t *node, cc_port_t *port)
{
	cc_port_t *peer_port = NULL;

	peer_port = cc_port_get(node, port->peer_port_id, strnlen(port->peer_port_id, CC_UUID_BUFFER_SIZE));
	if (peer_port == NULL)
		return CC_FAIL;

	if (port->tunnel != NULL) {
		cc_tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
	}

	if (peer_port->tunnel != NULL) {
		cc_tunnel_remove_ref(node, peer_port->tunnel);
		peer_port->tunnel = NULL;
	}

	port->peer_port = peer_port;
	strncpy(port->peer_id, node->id, CC_UUID_BUFFER_SIZE);
	cc_port_set_state(port, CC_PORT_ENABLED);
	peer_port->peer_port = port;
	strncpy(peer_port->peer_id, node->id, CC_UUID_BUFFER_SIZE);
	cc_port_set_state(peer_port, CC_PORT_ENABLED);

	cc_log_debug("'%s' connected to '%s'", port->id, port->peer_port_id);

	return CC_SUCCESS;
}

static cc_result_t cc_port_connect_with_pending_tunnel(cc_node_t *node, cc_port_t *port)
{
	if (port->tunnel->state == CC_TUNNEL_PENDING) {
		cc_log_debug("'%s' waiting for pending tunnel", port->id);
		return CC_SUCCESS;
	}

	if (port->tunnel->state == CC_TUNNEL_ENABLED) {
		if (cc_proto_send_port_connect(node, port, cc_port_connect_reply_handler) == CC_SUCCESS) {
			cc_port_set_state(port, CC_PORT_PENDING_CONNECT);
			return CC_SUCCESS;
		} else
			cc_log_error("Failed to send port connect");
	}

	cc_log_debug("Port: Removing tunnel ref");

	cc_tunnel_remove_ref(node, port->tunnel);
	port->tunnel = NULL;
	cc_port_set_state(port, CC_PORT_DISCONNECTED);

	return CC_FAIL;
}

static cc_result_t cc_port_get_tunnel(cc_node_t *node, cc_port_t *port)
{
	size_t peer_id_len = strnlen(port->peer_id, CC_UUID_BUFFER_SIZE);

	port->tunnel = cc_tunnel_get_from_peerid_and_type(node, port->peer_id, peer_id_len, CC_TUNNEL_TYPE_TOKEN);
	if (port->tunnel == NULL)
		port->tunnel = cc_tunnel_create(node, CC_TUNNEL_TYPE_TOKEN, CC_TUNNEL_DISCONNECTED, port->peer_id, peer_id_len, NULL, 0);

	if (port->tunnel == NULL) {
		cc_log_error("Failed to request tunnel to '%s'", port->peer_id);
		return CC_FAIL;
	}

	cc_tunnel_add_ref(port->tunnel);
	cc_port_set_state(port, CC_PORT_PENDING_TUNNEL);

	return CC_SUCCESS;
}

void cc_port_connect(cc_node_t *node, cc_port_t *port)
{
	if (port->state == CC_PORT_ENABLED) {
		cc_log_debug("Port already connected");
		return;
	}

	if (port->retries == 5) {
		cc_log("Port: Max connect attempts, deleting '%s'", port->id);
		cc_port_set_state(port, CC_PORT_DO_DELETE);
		return;
	}

	port->retries++;

	if (cc_port_connect_local(node, port) == CC_SUCCESS)
		return;

	if (port->tunnel == NULL) {
		if (strnlen(port->peer_id, CC_UUID_BUFFER_SIZE) > 0) {
			if (cc_port_get_tunnel(node, port) != CC_SUCCESS)
				cc_log_error("Failed to get tunnel");
		}
	}

	if (port->tunnel != NULL) {
		if (cc_port_connect_with_pending_tunnel(node, port) == CC_SUCCESS)
			return;
		cc_tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
	}

	memset(port->peer_id, 0, CC_UUID_BUFFER_SIZE);
	cc_port_set_state(port, CC_PORT_DISCONNECTED);

	cc_port_do_peer_lookup(node, port);
}

void cc_port_disconnect(cc_node_t *node, cc_port_t *port, bool unref_tunnel)
{
	if (port->tunnel != NULL && unref_tunnel) {
		if (cc_proto_send_port_disconnect(node, port, cc_port_disconnect_reply_handler) != CC_SUCCESS)
			cc_log_error("Failed to disconnect port");
		cc_tunnel_remove_ref(node, port->tunnel);
	}
	port->tunnel = NULL;

	if (port->peer_port != NULL) {
		cc_port_set_state(port->peer_port, CC_PORT_DISCONNECTED);
		port->peer_port = NULL;
	}
	memset(port->peer_id, 0, CC_UUID_BUFFER_SIZE);
	cc_port_set_state(port, CC_PORT_DISCONNECTED);
	cc_fifo_cancel(port->fifo);
}

void cc_port_transmit(cc_node_t *node, cc_port_t *port)
{
	cc_token_t *token = NULL;
	uint32_t sequencenbr = 0;

	if (port->state == CC_PORT_ENABLED) {
		if (port->actor->state == CC_ACTOR_ENABLED) {
			if (port->direction == CC_PORT_DIRECTION_OUT) {
				// send/move token
				if (cc_fifo_tokens_available(port->fifo, 1)) {
					cc_fifo_com_peek(port->fifo, &token, &sequencenbr);
					if (port->tunnel != NULL) {
						if (cc_proto_send_token(node, port, token, sequencenbr) != CC_SUCCESS)
							cc_fifo_com_cancel_read(port->fifo, sequencenbr);
					} else if (port->peer_port != NULL) {
						if (cc_fifo_write(port->peer_port->fifo, token->value, token->size) == CC_SUCCESS)
							cc_fifo_commit_read(port->fifo, false);
						else
							cc_fifo_cancel_commit(port->fifo);
					} else {
						cc_log_error("Port '%s' is enabled without a peer", port->id);
						cc_fifo_com_cancel_read(port->fifo, sequencenbr);
					}
				}
			}
		}
	} else if (port->state == CC_PORT_PENDING_TUNNEL) {
		if (port->tunnel != NULL && port->tunnel->state == CC_TUNNEL_ENABLED)
			cc_port_connect_with_pending_tunnel(node, port);
	}
}

char *cc_port_serialize_prev_connections(char *buffer, cc_port_t *port, const cc_node_t *node)
{
	size_t peer_id_len = strnlen(port->peer_id, CC_UUID_BUFFER_SIZE);

	buffer = cc_coder_encode_str(buffer, port->id, strnlen(port->id, CC_UUID_BUFFER_SIZE));
	buffer = cc_coder_encode_array(buffer, 1);
	buffer = cc_coder_encode_array(buffer, 2);
	if (peer_id_len == 0)
		buffer = cc_coder_encode_nil(buffer);
	else
		buffer = cc_coder_encode_str(buffer, port->peer_id, peer_id_len);
	buffer = cc_coder_encode_str(buffer, port->peer_port_id, strnlen(port->peer_port_id, CC_UUID_BUFFER_SIZE));

	return buffer;
}

char *cc_port_serialize_port(char *buffer, cc_port_t *port, bool include_state)
{
	unsigned int nbr_port_attributes = 4, i_token = 0;

	if (include_state)
		nbr_port_attributes += 1;

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
				if (port->direction == CC_PORT_DIRECTION_IN)
					buffer = cc_coder_encode_str(buffer, port->id, strnlen(port->id, CC_UUID_BUFFER_SIZE));
				else
					buffer = cc_coder_encode_str(buffer, port->peer_port_id, strnlen(port->peer_port_id, CC_UUID_BUFFER_SIZE));
			}
			buffer = cc_coder_encode_kv_uint(buffer, "N", port->fifo->size);
			buffer = cc_coder_encode_kv_map(buffer, "tentative_read_pos", 1);
			{
				if (port->direction == CC_PORT_DIRECTION_IN)
					buffer = cc_coder_encode_kv_uint(buffer, port->id, port->fifo->tentative_read_pos);
				else
					buffer = cc_coder_encode_kv_uint(buffer, port->peer_port_id, port->fifo->tentative_read_pos);
			}
			buffer = cc_coder_encode_kv_map(buffer, "read_pos", 1);
			{
				if (port->direction == CC_PORT_DIRECTION_IN)
					buffer = cc_coder_encode_kv_uint(buffer, port->id, port->fifo->tentative_read_pos);
				else
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
			if (port->direction == CC_PORT_DIRECTION_IN)
				buffer = cc_coder_encode_kv_str(buffer, "direction", "in", 2);
			else
				buffer = cc_coder_encode_kv_str(buffer, "direction", "out", 3);
			buffer = cc_coder_encode_kv_str(buffer, "routing", "default", 7);
		}
	}
	return buffer;
}
