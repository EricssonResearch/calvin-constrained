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
#include "proto.h"
#include "msgpuck/msgpuck.h"
#include "msgpack_helper.h"
#include "transport.h"
#include "platform.h"

#define STRING_TRUE "true"
#define STRING_FALSE "false"
#define STRING_IN "in"
#define STRING_OUT "out"
#define SEND_BUFFER_SIZE 1400

// TODO: Handle all commands and return proper error codes

result_t send_node_setup(char *msg_uuid, char *node_id, char *to_rt_uuid, uint32_t vid, uint32_t pid, transport_client_t *connection)
{
	char *buffer = NULL, *w = NULL;

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 6);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "from_rt_uuid", node_id);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid);
		w = encode_str(&w, "cmd", "PROXY_CONFIG");
		w = encode_uint(&w, "vid", vid);
		w = encode_uint(&w, "pid", pid);
	}

	return client_send(connection, buffer, w - buffer - 4);
}

result_t send_tunnel_request(char *msg_uuid, tunnel_t *tunnel, char *node_id, const char *type)
{
	char *buffer = NULL, *w = NULL;

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 7);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "from_rt_uuid", node_id);
		w = encode_str(&w, "to_rt_uuid", tunnel->peer_id);
		w = encode_str(&w, "cmd", "TUNNEL_NEW");
		w = encode_str(&w, "tunnel_id", tunnel->tunnel_id);
		w = encode_str(&w, "type", (char *)type);
		w = encode_map(&w, "policy", 0);
	}

	return client_send(tunnel->connection, buffer, w - buffer - 4);
}

result_t send_reply(transport_client_t *connection, char *msg_uuid, char *to_rt_uuid, char *from_rt_uuid, uint32_t status)
{
	char *buffer = NULL, *w = NULL;

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid);
		w = encode_str(&w, "from_rt_uuid", from_rt_uuid);
		w = encode_str(&w, "cmd", "REPLY");
		w = encode_map(&w, "value", 3);
		{
			w = encode_uint(&w, "status", status);
			w = encode_nil(&w, "data");
			w = encode_array(&w, "success_list", 7);
			w = mp_encode_uint(w, 200);
			w = mp_encode_uint(w, 201);
			w = mp_encode_uint(w, 202);
			w = mp_encode_uint(w, 203);
			w = mp_encode_uint(w, 204);
			w = mp_encode_uint(w, 205);
			w = mp_encode_uint(w, 206);
		}
	}

	return client_send(connection, buffer, w - buffer - 4);
}

result_t send_token(char *node_id, port_t *port, token_t *token)
{
	char *buffer = NULL, *w = NULL;

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", port->peer_id);
		w = encode_str(&w, "from_rt_uuid", node_id);
		w = encode_str(&w, "cmd", "TUNNEL_DATA");
		w = encode_str(&w, "tunnel_id", port->tunnel->tunnel_id);
		w = encode_map(&w, "value", 5);
		{
			w = encode_str(&w, "cmd", "TOKEN");
			w = encode_uint(&w, "sequencenbr", port->fifo->read_pos);
			w = encode_str(&w, "port_id", port->port_id);
			w = encode_str(&w, "peer_port_id", port->peer_port_id);
   		    w = encode_token(&w, token, true);
		}
	}

	return client_send(port->tunnel->connection, buffer, w - buffer - 4);
}

result_t send_token_reply(char *node_id, port_t *port, uint32_t sequencenbr, char *status)
{
	char *buffer = NULL, *w = NULL;

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", port->peer_id);
		w = encode_str(&w, "from_rt_uuid", node_id);
		w = encode_str(&w, "cmd", "TUNNEL_DATA");
		w = encode_str(&w, "tunnel_id", port->tunnel->tunnel_id);
		w = encode_map(&w, "value", 5);
		{
			w = encode_str(&w, "cmd", "TOKEN_REPLY");
			w = encode_uint(&w, "sequencenbr", sequencenbr);
			w = encode_str(&w, "peer_port_id", port->port_id);
			w = encode_str(&w, "port_id", port->peer_port_id);
			w = encode_str(&w, "value", status);
		}
	}

	return client_send(port->tunnel->connection, buffer, w - buffer - 4);
}

result_t send_port_connect(char *msg_uuid, char *node_id, port_t *port)
{
	char *buffer = NULL, *w = NULL;

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 11);
	{
		w = encode_str(&w, "to_rt_uuid", port->peer_id);
		w = encode_str(&w, "from_rt_uuid", node_id);
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "tunnel_id", port->tunnel->tunnel_id);
		w = encode_str(&w, "cmd", "PORT_CONNECT");
		w = encode_nil(&w, "peer_port_name");
		w = encode_nil(&w, "peer_actor_id");
		w = encode_str(&w, "peer_port_id", port->peer_port_id);
		w = encode_str(&w, "port_id", port->port_id);
		w = encode_nil(&w, "peer_port_properties");
		w = encode_map(&w, "port_properties", 3);
		w = encode_str(&w, "direction", port->direction == IN ? STRING_IN : STRING_OUT);
		w = encode_str(&w, "routing", "default");
		w = encode_uint(&w, "nbr_peers", 1);
	}

	return client_send(port->tunnel->connection, buffer, w - buffer - 4);
}

result_t send_port_disconnect(char *msg_uuid, char *node_id, port_t *port)
{
	char *buffer = NULL, *w = NULL;

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 10);
	{
		w = encode_str(&w, "to_rt_uuid", port->peer_id);
		w = encode_str(&w, "from_rt_uuid", node_id);
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "tunnel_id", port->tunnel->tunnel_id);
		w = encode_str(&w, "cmd", "PORT_DISCONNECT");
		w = encode_nil(&w, "peer_port_name");
		w = encode_nil(&w, "peer_actor_id");
		w = encode_str(&w, "peer_port_id", port->peer_port_id);
		w = encode_str(&w, "port_id", port->port_id);
		w = encode_nil(&w, "peer_port_dir");
	}

	return client_send(port->tunnel->connection, buffer, w - buffer - 4);
}

result_t send_set_node(char *msg_uuid, node_t *node)
{
	char *buffer = NULL, *w = NULL, key[50] = "", data[150] = "";

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	sprintf(key, "node-%s", node->node_id);
	// TODO: set uri and control uri when support for incoming connections and
	// the control api has been added
	sprintf(data,
		    "{\"attributes\": {\"indexed_public\": [\"/node/attribute/node_name/////%s\"], \"public\": {}}, \"control_uri\": \"%s\", \"uri\": [\"%s\"]}",
	        node->name,
	        "",
	        "");

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", node->storage_tunnel->peer_id);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "cmd", "TUNNEL_DATA");
		w = encode_str(&w, "tunnel_id", node->storage_tunnel->tunnel_id);
		w = encode_map(&w, "value", 4);
		{
			w = encode_str(&w, "cmd", "SET");
			w = encode_str(&w, "key", key);
			w = encode_str(&w, "value", data);
			w = encode_str(&w, "msg_uuid", msg_uuid);
		}
	}

	return client_send(node->storage_tunnel->connection, buffer, w - buffer - 4);
}

result_t send_remove_node(char *msg_uuid, node_t *node)
{
	char *buffer = NULL, *w = NULL, key[50] = "";

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	sprintf(key, "node-%s", node->node_id);

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", node->storage_tunnel->peer_id);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "cmd", "TUNNEL_DATA");
		w = encode_str(&w, "tunnel_id", node->storage_tunnel->tunnel_id);
		w = encode_map(&w, "value", 4);
		{
			w = encode_str(&w, "cmd", "SET");
			w = encode_str(&w, "key", key);
			w = encode_nil(&w, "value");
			w = encode_str(&w, "msg_uuid", msg_uuid);
		}
	}

	return client_send(node->storage_tunnel->connection, buffer, w - buffer - 4);
}

result_t send_set_actor(char *msg_uuid, tunnel_t *tunnel, char *node_id, actor_t *actor)
{
	int len = 0;
	char *buffer = NULL, *w = NULL, key[50] = "", data[400] = "", inports[100] = "", outports[100] = "";
	port_t *port = NULL;

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	port = actor->inports;
	while (port != NULL) {
		len += sprintf(inports + len,
		               "{\"id\": \"%s\", \"name\": \"%s\"}",
		               port->port_id,
		               port->port_name);
		port = port->next;
	}

	len = 0;
	port = actor->outports;
	while (port != NULL) {
		len += sprintf(outports + len,
		               "{\"id\": \"%s\", \"name\": \"%s\"}",
		               port->port_id,
		               port->port_name);
		port = port->next;
	}

	sprintf(key, "actor-%s", actor->id);
	len = sprintf(data,
	              "{\"is_shadow\": false, \"name\": \"%s\", \"node_id\": \"%s\", \"type\": \"%s\", \"inports\": [%s], \"outports\": [%s]}",
	              actor->name,
	              node_id,
	              actor->type,
	              inports,
	              outports);

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "from_rt_uuid", node_id);
		w = encode_str(&w, "to_rt_uuid", tunnel->peer_id);
		w = encode_str(&w, "cmd", "TUNNEL_DATA");
		w = encode_str(&w, "tunnel_id", tunnel->tunnel_id);
		w = encode_map(&w, "value", 4);
		{
			w = encode_str(&w, "cmd", "SET");
			w = encode_str(&w, "key", key);
			w = encode_str(&w, "value", data);
			w = encode_str(&w, "msg_uuid", msg_uuid);
		}
	}

	return client_send(tunnel->connection, buffer, w - buffer - 4);
}

result_t send_remove_actor(char *msg_uuid, tunnel_t *tunnel, char *node_id, actor_t *actor)
{
	char *buffer = NULL, *w = NULL, key[50];

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "from_rt_uuid", node_id);
		w = encode_str(&w, "to_rt_uuid", tunnel->peer_id);
		w = encode_str(&w, "cmd", "TUNNEL_DATA");
		w = encode_str(&w, "tunnel_id", tunnel->tunnel_id);
		w = encode_map(&w, "value", 4);
		{
			w = encode_str(&w, "cmd", "SET");
			w = encode_str(&w, "key", key);
			w = encode_nil(&w, "value");
			w = encode_str(&w, "msg_uuid", msg_uuid);
		}
	}

	return client_send(tunnel->connection, buffer, w - buffer - 4);
}

result_t send_set_port(char *msg_uuid, tunnel_t *tunnel, char *node_id, actor_t *actor, port_t *port)
{
	char *buffer = NULL, *w = NULL, key[50] = "", data[400] = "";

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	sprintf(key, "port-%s", port->port_id);
	sprintf(data,
	        "{\"properties\": {\"direction\": \"%s\", \"routing\": \"default\", \"nbr_peers\": 1}, \"name\": \"%s\", \"node_id\": \"%s\", \"connected\": %s, \"actor_id\": \"%s\"}",
	        port->direction == IN ? STRING_IN : STRING_OUT,
	        port->port_name,
	        node_id,
	        port->state == PORT_CONNECTED ? STRING_TRUE : STRING_FALSE,
	        actor->id);

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", tunnel->peer_id);
		w = encode_str(&w, "from_rt_uuid", node_id);
		w = encode_str(&w, "cmd", "TUNNEL_DATA");
		w = encode_str(&w, "tunnel_id", tunnel->tunnel_id);
		w = encode_map(&w, "value", 4);
		{
			w = encode_str(&w, "cmd", "SET");
			w = encode_str(&w, "key", key);
			w = encode_str(&w, "value", data);
			w = encode_str(&w, "msg_uuid", msg_uuid);
		}
	}

	return client_send(tunnel->connection, buffer, w - buffer - 4);
}

result_t send_remove_port(char *msg_uuid, tunnel_t *tunnel, char *node_id, port_t *port)
{
	char *buffer = NULL, *w = NULL, key[50] = "";

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	sprintf(key, "port-%s", port->port_id);

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", tunnel->peer_id);
		w = encode_str(&w, "from_rt_uuid", node_id);
		w = encode_str(&w, "cmd", "TUNNEL_DATA");
		w = encode_str(&w, "tunnel_id", tunnel->tunnel_id);
		w = encode_map(&w, "value", 4);
		{
			w = encode_str(&w, "cmd", "SET");
			w = encode_str(&w, "key", key);
			w = encode_nil(&w, "value");
			w = encode_str(&w, "msg_uuid", msg_uuid);
		}
	}

	return client_send(tunnel->connection, buffer, w - buffer - 4);
}

result_t send_actor_new(char *msg_uuid, char *to_rt_uuid, char *from_rt_uuid, actor_t *actor, transport_client_t *connection)
{
	char *buffer = NULL, *w = NULL;

	buffer = malloc(SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid);
		w = encode_str(&w, "from_rt_uuid", from_rt_uuid);
		w = encode_str(&w, "cmd", "ACTOR_NEW");
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = serialize_actor(actor, &w);
	}

	return client_send(connection, buffer, w - buffer - 4);
}

result_t parse_tunnel_new_reply(node_t *node, char *root, char *msg_uuid)
{
	result_t result = FAIL;
	uint32_t status = 0;
	char *tunnel_id = NULL, *value = NULL, *data = NULL, *r = root;

	if (get_value_from_map(&r, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(&value, "status", &status) == SUCCESS) {
			if (status == 200) {
				if (get_value_from_map(&value, "data", &data) == SUCCESS) {
					if (decode_string_from_map(&data, "tunnel_id", &tunnel_id) == SUCCESS) {
						result = tunnel_connected(tunnel_id);
						free(tunnel_id);
					}
				}
			} else {
				log_error("Failed to connect tunnel, status '%lu'", (unsigned long)status);
			}
		}
	}

	return result;
}

result_t parse_port_connected(node_t *node, char *root)
{
	result_t result = FAIL;
	char *value = NULL, *data = NULL, *r = root;
	uint32_t status = 0;
	char *port_id = NULL;

	if (get_value_from_map(&r, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(&value, "status", &status) == SUCCESS) {
			if (status == 200) {
				if (get_value_from_map(&value, "data", &data) == SUCCESS) {
					if (decode_string_from_map(&data, "port_id", &port_id) == SUCCESS) {
						result = port_connected(port_id);
					}
				}
			}
		}
	}

	if (port_id != NULL) {
		free(port_id);
	}

	return result;
}

result_t parse_node_setup_reply(node_t *node, char *root)
{
	result_t result = FAIL;
	char *value = NULL, *r = root;
	uint32_t status = 0;

	if (get_value_from_map(&r, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(&value, "status", &status) == SUCCESS) {
			if (status == 200) {
				log_debug("Node configured");
				result = SUCCESS;
			} else {
				log_error("Failed to configure node, status '%lu'", (unsigned long)status);
			}
		}
	}

	return result;
}

result_t parse_actor_new_reply(node_t *node, char *root, actor_t *actor)
{
	result_t result = FAIL;
	char *value = NULL, *r = root;
	uint32_t status = 0;

	if (get_value_from_map(&r, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(&value, "status", &status) == SUCCESS) {
			result = SUCCESS;
			if (status == 200) {
				log_debug("Actor '%s' migrated", actor->id);
				delete_actor(actor, false);
			} else {
				log_error("Failed to migrate actor '%s'", actor->id);
			}
		}
	}

	return result;
}

result_t parse_port_disconnect_reply(node_t *node, char *root, char *port_id)
{
	result_t result = FAIL;
	char *value = NULL, *r = root;
	uint32_t status = 0;

	if (get_value_from_map(&r, "value", &value) == SUCCESS) {
		if (decode_uint_from_map(&value, "status", &status) == SUCCESS) {
			result = SUCCESS;
			if (status == 200) {
				log_debug("Port '%s' disconnected", port_id);
			} else {
				log_error("Failed to disconnect port '%s'", port_id);
			}
		}
	}

	return result;
}

result_t parse_reply(node_t *node, char *root)
{
	result_t result = FAIL;
	char *msg_uuid = NULL, *r = root;
	pending_msg_t *pending_msgs = node->pending_msgs;

	if (decode_string_from_map(&r, "msg_uuid", &msg_uuid) == SUCCESS) {
		while (pending_msgs != NULL) {
			if (strcmp(pending_msgs->msg_uuid, msg_uuid) == 0) {
				if (pending_msgs->type == PROXY_CONFIG) {
					result = parse_node_setup_reply(node, root);
				} else if (pending_msgs->type == TUNNEL_NEW) {
					result = parse_tunnel_new_reply(node, root, msg_uuid);
				} else if (pending_msgs->type == PORT_CONNECT) {
					result = parse_port_connected(node, root);
				} else if (pending_msgs->type == ACTOR_NEW) {
					result = parse_actor_new_reply(node, root, (actor_t *)pending_msgs->data);
				} else if (pending_msgs->type == PORT_DISCONNECT) {
					result = parse_port_disconnect_reply(node, root, (char *)pending_msgs->data);
				} else {
					log_error("Unhandled message type '%d'", pending_msgs->type);
				}
				remove_pending_msg(msg_uuid);
				break;
			}
			pending_msgs = pending_msgs->next;
		}
		free(msg_uuid);
	}

	return result;
}

result_t parse_token(node_t *node, char *root)
{
	result_t result = FAIL;
	char *obj_value = NULL, *obj_token = NULL, *from_rt_uuid = NULL, *port_id = NULL, *r = root;
	uint32_t sequencenbr = 0;
	token_t *token = NULL;

	if (decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid) == SUCCESS) {
		if (get_value_from_map(&r, "value", &obj_value) == SUCCESS) {
			if (decode_string_from_map(&obj_value, "peer_port_id", &port_id) == SUCCESS) {
				if (decode_uint_from_map(&obj_value, "sequencenbr", &sequencenbr) == SUCCESS) {
					if (get_value_from_map(&obj_value, "token", &obj_token) == SUCCESS) {
						if (decode_token(obj_token, &token) == SUCCESS) {
							result = handle_token(port_id, token, sequencenbr);
						} else {
							log_error("Failed to create token");
						}
					}
				}
				free(port_id);
			}
		}
		free(from_rt_uuid);
	}

	return result;
}

result_t parse_token_reply(node_t *node, char *root)
{
	result_t result = FAIL;
	char *value = NULL, *r = root, *port_id = NULL, *status = NULL;
	uint32_t sequencenbr = 0;

	if (get_value_from_map(&r, "value", &value) == SUCCESS) {
		if (decode_string_from_map(&value, "port_id", &port_id) == SUCCESS) {
			if (decode_string_from_map(&value, "value", &status) == SUCCESS) {
				if (decode_uint_from_map(&value, "sequencenbr", &sequencenbr) == SUCCESS) {
					if (strcmp(status, "ACK") == 0) {
						handle_token_reply(port_id, true);
						result = SUCCESS;
					} else {
						handle_token_reply(port_id, false);
						log_error("Received '%s' for token with sequencenbr '%ld' on port '%s'",
							status, (unsigned long)sequencenbr, port_id);
					}
				}
				free(status);
			}
			free(port_id);
		}
	}

	return result;
}

result_t parse_tunnel_data(node_t *node, char *root)
{
	char *msg_uuid = NULL, *value = NULL, *key = NULL, *cmd = NULL, *r = root;
	pending_msg_t *pending_msgs = node->pending_msgs;
	bool status = false;

	if (get_value_from_map(&r, "value", &value) == SUCCESS) {
		if (has_key(&value, "msg_uuid")) {
			if (decode_string_from_map(&value, "msg_uuid", &msg_uuid) == SUCCESS) {
				while (pending_msgs != NULL) {
					if (strcmp(pending_msgs->msg_uuid, msg_uuid) == 0) {
						if (pending_msgs->type == STORAGE_SET) {
							if (decode_string_from_map(&value, "key", &key) == SUCCESS) {
								if (decode_bool_from_map(&value, "value", &status) == SUCCESS) {
									if (!status) {
										log_error("Failed to store '%s'", key);
									}
								}
								free(key);
							}
						} else {
							log_error("Unhandled type '%d'", pending_msgs->type);
						}
						remove_pending_msg(msg_uuid);
						break;
					}
					pending_msgs = pending_msgs->next;
				}
				free(msg_uuid);
				return status ? SUCCESS : FAIL;
			}
		} else if (has_key(&value, "cmd")) {
			if (decode_string_from_map(&value, "cmd", &cmd) == SUCCESS) {
				log_debug("Received %s on tunnel", cmd);
				if (strcmp(cmd, "TOKEN") == 0) {
					free(cmd);
					return parse_token(node, root);
				} else if(strcmp(cmd, "TOKEN_REPLY") == 0) {
					free(cmd);
					return parse_token_reply(node, root);
				} else {
					log_error("Unhandled tunnel cmd '%s'", cmd);
				}
				free(cmd);
			}
		} else {
			log_error("Unknown message");
		}
	}

	return FAIL;
}

result_t parse_actor_new(node_t *node, char *root, transport_client_t *connection)
{
	result_t result = SUCCESS;
	actor_t *actor = NULL;
	char *from_rt_uuid = NULL, *msg_uuid = NULL, *r = root;

	if (decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid) == SUCCESS) {
		if (decode_string_from_map(&r, "msg_uuid", &msg_uuid) == SUCCESS) {
			if (create_actor(root, &actor) != SUCCESS) {
				log_error("Failed to create actor");
				result = FAIL;
			}
		}
	}

	if (result == SUCCESS) {
		result = send_reply(connection, msg_uuid, from_rt_uuid, node->node_id, 200);
	} else {
		result = send_reply(connection, msg_uuid, from_rt_uuid, node->node_id, 500);
		if (actor != NULL) {
			delete_actor(actor, true);
		}
	}

	if (from_rt_uuid != NULL) {
		free(from_rt_uuid);
	}

	if (msg_uuid != NULL) {
		free(msg_uuid);
	}

	return result;
}

result_t parse_actor_migrate(node_t *node, char *root, transport_client_t *connection)
{
	result_t result = SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *to_rt_uuid = NULL, *actor_id = NULL, *msg_uuid = NULL;
	actor_t *actor = NULL;

	result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "to_rt_uuid", &to_rt_uuid);
	}

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "actor_id", &actor_id);
	}

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);
	}

	if (result == SUCCESS) {
		actor = get_actor(actor_id);
		if (actor != NULL) {
			if (disconnect_actor(actor) == SUCCESS) {
				if (send_actor_new(msg_uuid, from_rt_uuid, node->node_id, actor, connection) == SUCCESS) {
					add_pending_msg_with_data(ACTOR_NEW, strdup(msg_uuid), (void *)actor);
				} else {
					log_error("Failed to migrate actor");
				}
			} else {
				log_error("Failed to disconnect actor '%s'", actor->id);
			}
		} else {
			log_error("No actor with id '%s'", actor_id);
			result = FAIL;
		}
	}

	if (result == SUCCESS) {
		result = send_reply(connection, msg_uuid, from_rt_uuid, node->node_id, 200);
	} else {
		result = send_reply(connection, msg_uuid, from_rt_uuid, node->node_id, 500);
	}

	if (from_rt_uuid != NULL) {
		free(from_rt_uuid);
	}

	if (to_rt_uuid != NULL) {
		free(to_rt_uuid);
	}

	if (actor_id != NULL) {
		free(actor_id);
	}

	if (msg_uuid != NULL) {
		free(msg_uuid);
	}

	return result;
}

result_t parse_app_destroy(node_t *node, char *root, transport_client_t *connection)
{
	result_t result = SUCCESS;

	char *r = root, *from_rt_uuid = NULL, *to_rt_uuid = NULL, *msg_uuid = NULL;
	char *obj_actor_uuids = NULL, *actor_id = NULL;
	uint32_t i = 0, size = 0;
	actor_t *actor = NULL;

	result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "to_rt_uuid", &to_rt_uuid);
	}

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);
	}

	if (result == SUCCESS) {
		result = get_value_from_map(&r, "actor_uuids", &obj_actor_uuids);
		if (result == SUCCESS) {
			size = mp_decode_array((const char **)&obj_actor_uuids);
			for (i = 0; i < size; i++) {
				result = decode_str(&obj_actor_uuids, &actor_id);
				if (result == SUCCESS) {
					actor = get_actor(actor_id);
					delete_actor(actor, true);
					free(actor_id);
				}
			}
		}
	}

	if (result == SUCCESS) {
		result = send_reply(connection, msg_uuid, from_rt_uuid, node->node_id, 200);
	} else {
		result = send_reply(connection, msg_uuid, from_rt_uuid, node->node_id, 500);
	}

	if (from_rt_uuid != NULL) {
		free(from_rt_uuid);
	}

	if (to_rt_uuid != NULL) {
		free(to_rt_uuid);
	}

	if (msg_uuid != NULL) {
		free(msg_uuid);
	}

	return result;
}

result_t parse_port_disconnect(node_t *node, char *root, transport_client_t *connection)
{
	result_t result = SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *to_rt_uuid = NULL, *msg_uuid = NULL, *peer_port_id = NULL;

	result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "to_rt_uuid", &to_rt_uuid);
	}

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);
	}

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "peer_port_id", &peer_port_id);
	}

	if (result == SUCCESS) {
		result = handle_port_disconnect(peer_port_id);
	}

	if (result == SUCCESS) {
		result = send_reply(connection, msg_uuid, from_rt_uuid, node->node_id, 200);
	} else {
		result = send_reply(connection, msg_uuid, from_rt_uuid, node->node_id, 500);
	}

	if (from_rt_uuid != NULL) {
		free(from_rt_uuid);
	}

	if (to_rt_uuid != NULL) {
		free(to_rt_uuid);
	}

	if (msg_uuid != NULL) {
		free(msg_uuid);
	}

	if (peer_port_id != NULL) {
		free(peer_port_id);
	}

	return result;
}

result_t parse_port_connect(node_t *node, char *root, transport_client_t *connection)
{
	result_t result = SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *to_rt_uuid = NULL, *msg_uuid = NULL;
	char *port_id = NULL, *peer_port_id = NULL;

	result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "to_rt_uuid", &to_rt_uuid);
	}

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);
	}

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "port_id", &port_id);
	}

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "peer_port_id", &peer_port_id);
	}

	if (result == SUCCESS) {
		result = handle_port_connect(port_id, peer_port_id);
	}

	if (result == SUCCESS) {
		result = send_reply(connection, msg_uuid, from_rt_uuid, node->node_id, 200);
	} else {
		result = send_reply(connection, msg_uuid, from_rt_uuid, node->node_id, 404);
	}

	if (from_rt_uuid != NULL) {
		free(from_rt_uuid);
	}

	if (to_rt_uuid != NULL) {
		free(to_rt_uuid);
	}

	if (msg_uuid != NULL) {
		free(msg_uuid);
	}

	if (port_id != NULL) {
		free(port_id);
	}

	if (peer_port_id != NULL) {
		free(peer_port_id);
	}

	return result;
}

result_t parse_message(node_t *node, char *data, transport_client_t *connection)
{
	result_t result = SUCCESS;
	char *cmd = NULL, *r = data, *msg_uuid = NULL, *from_rt_uuid = NULL;

	result = decode_string_from_map(&r, "cmd", &cmd);
	if (result == SUCCESS) {
		log_debug("Received '%s'", cmd);
		if (strcmp(cmd, "REPLY") == 0) {
			result = parse_reply(node, r);
		} else if (strcmp(cmd, "TUNNEL_DATA") == 0) {
			result = parse_tunnel_data(node, r);
		} else if (strcmp(cmd, "ACTOR_NEW") == 0) {
			result = parse_actor_new(node, r, connection);
		}  else if (strcmp(cmd, "ACTOR_MIGRATE") == 0) {
			result = parse_actor_migrate(node, r, connection);
		}  else if (strcmp(cmd, "APP_DESTROY") == 0) {
			result = parse_app_destroy(node, r, connection);
		} else if (strcmp(cmd, "PORT_DISCONNECT") == 0) {
			result = parse_port_disconnect(node, r, connection);
		} else if (strcmp(cmd, "PORT_CONNECT") == 0) {
			result = parse_port_connect(node, r, connection);
		} else {
			log_error("Unhandled command '%s'", cmd);
			result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);
			if (result == SUCCESS) {
				result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);
				if (result == SUCCESS) {
					result = send_reply(connection, msg_uuid, from_rt_uuid, node->node_id, 500);
					free(from_rt_uuid);
				}
				free(msg_uuid);
			}
			result = FAIL;
		}
		free(cmd);
	}

	return result;
}