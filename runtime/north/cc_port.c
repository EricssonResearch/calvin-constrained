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
#include "coder/cc_coder.h"
#include "../south/platform/cc_platform.h"

static cc_result_t port_setup_connection(cc_node_t *node, cc_port_t *port, char *peer_id, uint32_t peer_id_len);
cc_port_t *cc_port_get_from_peer_port_id(struct cc_node_t *node, const char *peer_port_id, uint32_t peer_port_id_len);
void port_set_state(cc_port_t *port, cc_port_state_t state);

static cc_result_t port_remove_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	return CC_SUCCESS;
}

static cc_result_t port_get_peer_port_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	cc_port_t *port = NULL;
	char *value = NULL, *value_value = NULL, *node_id = NULL;
	char *tmp = NULL, *value_key = NULL;
	uint32_t value_value_len = 0, value_key_len = 0, status = 0;
	size_t tmp_len = 0;

	if (cc_coder_get_value_from_map(data, "value", &value) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_has_key(value, "status")) {
		if (cc_coder_decode_uint_from_map(value, "status", &status) != CC_SUCCESS)
			return CC_FAIL;

		if (status != 200) {
			cc_log_error("Failed to get port info");
			return CC_SUCCESS;
		}
	}

	if (cc_coder_decode_string_from_map(value, "key", &value_key, &value_key_len) != CC_SUCCESS)
		return CC_FAIL;

	port = cc_port_get_from_peer_port_id(node, value_key + 5, value_key_len - 5);
	if (port == NULL)
		return CC_SUCCESS;

	if (cc_coder_decode_string_from_map(value, "value", &value_value, &value_value_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_get_json_string_value(value_value, value_value_len, (char *)"node_id", 7, &tmp, &tmp_len) != CC_SUCCESS) {
		cc_log_error("No attribute 'node_id'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&node_id, tmp_len + 1) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	strncpy(node_id, tmp, tmp_len);
	node_id[tmp_len] = '\0';

	port_setup_connection(node, port, node_id, strnlen(node_id, CC_UUID_BUFFER_SIZE));
	cc_platform_mem_free((void *)node_id);

	return CC_SUCCESS;
}

static void port_enable(cc_port_t *port)
{
	port_set_state(port, CC_PORT_ENABLED);
	cc_actor_port_enabled(port->actor);
}

static cc_result_t port_connect_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	char *value = NULL, *value_data = NULL, *peer_port_id = NULL;
	uint32_t status = 0, peer_port_id_len = 0;
	cc_port_t *port = NULL;

	if (cc_coder_get_value_from_map(data, "value", &value) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_uint_from_map(value, "status", &status) != CC_SUCCESS)
		return CC_FAIL;

	if (status == 200) {
		if (cc_coder_get_value_from_map(value, "data", &value_data) != CC_SUCCESS)
			return CC_FAIL;

		if (cc_coder_decode_string_from_map(value_data, "port_id", &peer_port_id, &peer_port_id_len) != CC_SUCCESS)
			return CC_FAIL;

		port = cc_port_get_from_peer_port_id(node, peer_port_id, peer_port_id_len);
		if (port != NULL)
			port_enable(port);
	}

	return CC_SUCCESS;
}

static cc_result_t port_store_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	char *value = NULL;
	bool status = false;

	if (cc_coder_get_value_from_map(data, "value", &value) == CC_SUCCESS) {
		if (cc_coder_decode_bool_from_map(value, "value", &status) == CC_SUCCESS) {
			if (cc_coder_decode_bool_from_map(value, "value", &status) == CC_SUCCESS) {
				if (status != true)
					cc_log_error("Failed to store port '%s'", (char *)msg_data);
				return CC_SUCCESS;
			}
		}
	}

	cc_log_error("Failed to decode message");
	return CC_FAIL;
}

static cc_result_t cc_port_disconnect_reply_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	cc_log_debug("Port '%s' disconnected", msg_data);
	return CC_SUCCESS;
}

void port_set_state(cc_port_t *port, cc_port_state_t state)
{
	cc_log_debug("Port '%s' state '%d' -> '%d'", port->id, port->state, state);
	port->state = state;
}

cc_port_t *cc_port_create(cc_node_t *node, cc_actor_t *actor, char *obj_port, char *obj_prev_connections, cc_port_direction_t direction)
{
	char *obj_prev_ports = NULL, *obj_prev_port = NULL, *obj_peer = NULL, *obj_queue = NULL, *obj_properties = NULL;
	char *r = obj_port, *port_id = NULL, *port_name = NULL, *routing = NULL, *peer_id = NULL, *peer_port_id = NULL, *tmp_value = NULL;
	uint32_t nbr_peers = 0, port_id_len = 0, port_name_len = 0, routing_len = 0, peer_id_len = 0, peer_port_id_len = 0;
	cc_port_t *port = NULL;

	if (cc_coder_decode_string_from_map(r, "id", &port_id, &port_id_len) != CC_SUCCESS)
		return NULL;

	if (cc_coder_decode_string_from_map(r, "name", &port_name, &port_name_len) != CC_SUCCESS)
		return NULL;

	if (cc_coder_get_value_from_map(r, "properties", &obj_properties) != CC_SUCCESS)
		return NULL;

	if (cc_coder_decode_string_from_map(obj_properties, "routing", &routing, &routing_len) != CC_SUCCESS)
		return NULL;

	if (strncmp("default", routing, routing_len) != 0 && strncmp("fanout", routing, routing_len) != 0) {
		cc_log_error("Unsupported routing");
		return NULL;
	}

	if (cc_coder_decode_uint_from_map(obj_properties, "nbr_peers", &nbr_peers) != CC_SUCCESS)
		return NULL;

	if (nbr_peers != 1) {
		cc_log_error("Only one peer is supported");
		return NULL;
	}

	if (cc_coder_get_value_from_map(r, "queue", &obj_queue) != CC_SUCCESS)
		return NULL;

	if (cc_coder_get_value_from_map(obj_prev_connections,
		direction == CC_PORT_DIRECTION_IN ? "inports" : "outports",
		&obj_prev_ports) != CC_SUCCESS)
		return NULL;

	if (cc_coder_get_value_from_map_n(obj_prev_ports, port_id, port_id_len, &obj_prev_port) != CC_SUCCESS)
		return NULL;

	if (cc_coder_get_value_from_array(obj_prev_port, 0, &obj_peer) != CC_SUCCESS)
		return NULL;

	if (cc_coder_get_value_from_array(obj_peer, 0, &tmp_value) != CC_SUCCESS)
		return NULL;

	if (cc_coder_type_of(tmp_value) == CC_CODER_STR) {
		if (cc_coder_decode_str(tmp_value, &peer_id, &peer_id_len) != CC_SUCCESS)
			return NULL;
	}

	if (cc_coder_decode_string_from_array(obj_peer, 1, &peer_port_id, &peer_port_id_len) != CC_SUCCESS)
		return NULL;

	if (cc_platform_mem_alloc((void **)&port, sizeof(cc_port_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	memset(port, 0, sizeof(cc_port_t));

	port->direction = direction;
	port->state = CC_PORT_DISCONNECTED;
	port->actor = actor;
	port->peer_port = NULL;
	port->tunnel = NULL;
	strncpy(port->id, port_id, port_id_len);
	strncpy(port->name, port_name, port_name_len);
	strncpy(port->peer_port_id, peer_port_id, peer_port_id_len);

	port->fifo = cc_fifo_init(obj_queue);
	if (port->fifo ==  NULL) {
		cc_log_error("Failed to init fifo");
		cc_port_free(node, port, false);
		return NULL;
	}

	if (peer_id != NULL) {
		port->tunnel = cc_tunnel_get_from_peerid_and_type(node, peer_id, peer_id_len, CC_TUNNEL_TYPE_TOKEN);
		if (port->tunnel != NULL) {
			port_set_state(port, CC_PORT_PENDING);
			cc_actor_port_enabled(actor);
		}
		cc_tunnel_add_ref(port->tunnel);
	}

	if (cc_coder_has_key(r, "constrained_state")) {
		if (cc_coder_decode_uint_from_map(r, "constrained_state", (uint32_t *)&port->state) != CC_SUCCESS)
			return NULL;
		if (port->state == CC_PORT_ENABLED)
			cc_actor_port_enabled(port->actor);
	} else {
		if (node->transport_client != NULL) {
			if (cc_proto_send_set_port(node, port, port_store_reply_handler) != CC_SUCCESS) {
				cc_log_error("Failed to store port");
				cc_port_free(node, port, false);
				return NULL;
			}

			if (port_setup_connection(node, port, peer_id, peer_id_len) != CC_SUCCESS) {
				cc_log_error("Failed setup connections");
				cc_port_free(node, port, true);
				return NULL;
			}
		}
	}

	cc_log_debug("Created port '%s' with name '%s' and direction '%s'", port->id, port->name, port->direction == CC_PORT_DIRECTION_IN ? "in" : "out");

	return port;
}

void cc_port_free(cc_node_t *node, cc_port_t *port, bool remove_from_registry)
{
	cc_log_debug("Deleting port '%s'", port->id);

	if (remove_from_registry) {
		if (cc_proto_send_remove_port(node, port, port_remove_reply_handler) != CC_SUCCESS)
			cc_log_error("Failed to remove port '%s'", port->id);
	}

	if (port->tunnel != NULL)
		cc_tunnel_remove_ref(node, port->tunnel);

	cc_fifo_free(port->fifo);

	cc_platform_mem_free((void *)port);
}

char *cc_port_get_peer_id(const cc_node_t *node, cc_port_t *port)
{
	if (port->peer_port != NULL)
		return (char *)node->id;

	if (port->tunnel != NULL)
		return port->tunnel->link->peer_id;

	return NULL;
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
	cc_list_t *actors = node->actors;
	cc_actor_t *actor = NULL;
	cc_list_t *ports = NULL;
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
			port_set_state(port, CC_PORT_ENABLED);
			cc_actor_port_enabled(port->actor);
			return CC_SUCCESS;
		}
		cc_tunnel_remove_ref(node, port->tunnel);
	}

	port->tunnel = cc_tunnel_get_from_id(node, tunnel_id, tunnel_id_len);
	if (port->tunnel == NULL) {
		cc_log_error("Failed to get tunnel");
		return CC_FAIL;
	}

	cc_tunnel_add_ref(port->tunnel);
	port_set_state(port, CC_PORT_ENABLED);
	cc_actor_port_enabled(port->actor);
	cc_log_debug("Port '%s' connected by remote", port->id);

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
	port_set_state(port, CC_PORT_DISCONNECTED);
	cc_actor_port_disconnected(port->actor);

	if (port->tunnel != NULL) {
		cc_tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
	}

	cc_log_debug("Port '%s' disconnected by remote", port->id);

	return CC_SUCCESS;
}

static cc_result_t port_setup_connection(cc_node_t *node, cc_port_t *port, char *peer_id, uint32_t peer_id_len)
{
	cc_port_t *peer_port = NULL;

	// handle pending tunnel
	if (port->tunnel != NULL) {
		if (port->tunnel->state == CC_TUNNEL_PENDING)
			return CC_SUCCESS;
		else if (port->tunnel->state == CC_TUNNEL_ENABLED) {
			if (cc_proto_send_port_connect(node, port, port_connect_reply_handler) == CC_SUCCESS) {
				port_set_state(port, CC_PORT_PENDING);
				return CC_SUCCESS;
			}
		}
		cc_tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
		port_set_state(port, CC_PORT_DISCONNECTED);
	}

	// handle local connection
	peer_port = cc_port_get(node, port->peer_port_id, strnlen(port->peer_port_id, CC_UUID_BUFFER_SIZE));
	if (peer_port != NULL) {
		if (peer_port->tunnel != NULL) {
			cc_tunnel_remove_ref(node, peer_port->tunnel);
			peer_port->tunnel = NULL;
		}
		port->peer_port = peer_port;
		port_set_state(port, CC_PORT_ENABLED);
		port->tunnel = NULL;
		peer_port->peer_port = port;
		port_set_state(peer_port, CC_PORT_ENABLED);
		cc_log_debug("Port '%s' connected to local port '%s'", port->id, port->peer_port_id);
		return CC_SUCCESS;
	}

	// get/create a tunnel
	if (peer_id != NULL) {
		port->tunnel = cc_tunnel_get_from_peerid_and_type(node, peer_id, peer_id_len, CC_TUNNEL_TYPE_TOKEN);
		if (port->tunnel == NULL) {
			port->tunnel = cc_tunnel_create(node, CC_TUNNEL_TYPE_TOKEN, CC_TUNNEL_DISCONNECTED, peer_id, peer_id_len, NULL, 0);
			if (port->tunnel == NULL) {
				cc_log_error("Failed to create a tunnel for port '%s'", port->id);
				return CC_FAIL;
			}
		}
		port->state = CC_PORT_PENDING;
		cc_tunnel_add_ref(port->tunnel);
		return CC_SUCCESS;
	}

	// lookup peer
	if (port->state == CC_PORT_DISCONNECTED) {
		if (cc_proto_send_get_port(node, port->peer_port_id, port_get_peer_port_reply_handler, port->id) == CC_SUCCESS) {
			cc_log_debug("Doing peer lookup for port '%s' with peer port '%s'", port->id, port->peer_port_id);
			port_set_state(port, CC_PORT_PENDING);
			return CC_SUCCESS;
		}
	}

	return CC_FAIL;
}

void cc_port_disconnect(cc_node_t *node, cc_port_t *port)
{
	if (port->tunnel != NULL) {
		if (cc_proto_send_cc_port_disconnect(node, port, cc_port_disconnect_reply_handler) != CC_SUCCESS)
			cc_log_error("Failed to disconnect port");
		cc_tunnel_remove_ref(node, port->tunnel);
		port->tunnel = NULL;
	}

	port->peer_port = NULL;
	port_set_state(port, CC_PORT_DISCONNECTED);
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
						cc_log_error("Port '%s' is enabled without a peer", port->name);
						cc_fifo_com_cancel_read(port->fifo, sequencenbr);
					}
				}
			}
		}
	} else
		port_setup_connection(node, port, NULL, 0);
}

char *cc_port_serialize_prev_connections(char *buffer, cc_port_t *port, const cc_node_t *node)
{
	char *peer_id = NULL;

	peer_id = cc_port_get_peer_id(node, port);
	buffer = cc_coder_encode_str(buffer, port->id, strnlen(port->id, CC_UUID_BUFFER_SIZE));
	buffer = cc_coder_encode_array(buffer, 1);
	buffer = cc_coder_encode_array(buffer, 2);
	if (peer_id != NULL)
		buffer = cc_coder_encode_str(buffer, peer_id, strnlen(peer_id, CC_UUID_BUFFER_SIZE));
	else
		buffer = cc_coder_encode_nil(buffer);
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
