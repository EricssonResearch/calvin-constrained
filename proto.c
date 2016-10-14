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
#include "link.h"

#define STRING_TRUE "true"
#define STRING_FALSE "false"
#define STRING_IN "in"
#define STRING_OUT "out"
#define NBR_OF_COMMANDS 9

struct command_handler_t {
	char command[50];
	result_t (*handler)(node_t *node, char *data);
};

static result_t parse_reply(node_t *node, char *data);
static result_t parse_tunnel_data(node_t *node, char *data);
static result_t parse_actor_new(node_t *node, char *data);
static result_t parse_actor_migrate(node_t *node, char *data);
static result_t parse_app_destroy(node_t *node, char *data);
static result_t parse_port_disconnect(node_t *node, char *data);
static result_t parse_port_connect(node_t *node, char *data);
static result_t parse_tunnel_new(node_t *node, char *data);
static result_t parse_route_request(node_t *node, char *data);

struct command_handler_t command_handlers[NBR_OF_COMMANDS] = {
	{"REPLY", parse_reply},
	{"TUNNEL_DATA", parse_tunnel_data},
	{"ACTOR_NEW", parse_actor_new},
	{"ACTOR_MIGRATE", parse_actor_migrate},
	{"APP_DESTROY", parse_app_destroy},
	{"PORT_DISCONNECT", parse_port_disconnect},
	{"PORT_CONNECT", parse_port_connect},
	{"TUNNEL_NEW", parse_tunnel_new},
	{"ROUTE_REQUEST", parse_route_request}
};

result_t send_node_setup(const node_t *node, result_t (*handler)(char*, void*))
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL, *msg_uuid = NULL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, NULL) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	buffer = malloc(250);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 7);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id);
		w = encode_str(&w, "cmd", "PROXY_CONFIG");
		w = encode_uint(&w, "vid", node->vid);
		w = encode_uint(&w, "pid", node->pid);
		w = encode_str(&w, "name", node->name);
	}

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent PROXY_CONFIG with msg_uuid '%s'", msg_uuid);
	else {
		log_debug("Failed to send PROXY_CONFIG");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_route_request(const node_t *node, char *dest_peer_id, result_t (*handler)(char*, void*))
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL, *msg_uuid = NULL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, NULL) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	buffer = malloc(300);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 6);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id);
		w = encode_str(&w, "cmd", "ROUTE_REQUEST");
		w = encode_str(&w, "dest_peer_id", dest_peer_id);
		w = encode_str(&w, "org_peer_id", node->node_id);
	}

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent ROUTE_REQUEST with msg_uuid '%s'", msg_uuid);
	else {
		log_debug("Failed to send ROUTE_REQUEST");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_tunnel_request(const node_t *node, tunnel_t *tunnel, result_t (*handler)(char*, void*))
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL, *msg_uuid = NULL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, NULL) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	buffer = malloc(300);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 7);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "to_rt_uuid", tunnel->link->peer_id);
		w = encode_str(&w, "cmd", "TUNNEL_NEW");
		w = encode_str(&w, "tunnel_id", tunnel->tunnel_id);
		w = encode_str(&w, "type", tunnel->type == TUNNEL_TYPE_STORAGE ? STORAGE_TUNNEL : TOKEN_TUNNEL);
		w = encode_map(&w, "policy", 0);
	}

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent TUNNEL_NEW with msg_uuid '%s'", msg_uuid);
	else {
		log_debug("Failed to send TUNNEL_NEW");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_tunnel_destroy(const node_t *node, const char *to_rt_uuid, const char *tunnel_id, result_t (*handler)(char*, void*))
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL, *msg_uuid = NULL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, NULL) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	buffer = malloc(250);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid);
		w = encode_str(&w, "cmd", "TUNNEL_DESTROY");
		w = encode_str(&w, "tunnel_id", tunnel_id);
	}

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent TUNNEL_DESTROY with msg_uuid '%s'", msg_uuid);
	else {
		log_debug("Failed to send TUNNEL_DESTROY");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_reply(const node_t *node, char *msg_uuid, char *to_rt_uuid, uint32_t status)
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL;

	buffer = malloc(250);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return result;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
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

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent REPLY with msg_uuid '%s'", msg_uuid);
	else
		log_debug("Failed to send REPLY");

	return result;
}

result_t send_tunnel_new_reply(const node_t *node, char *msg_uuid, char *to_rt_uuid, uint32_t status, char *tunnel_id)
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL;

	buffer = malloc(300);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return result;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "cmd", "REPLY");
		w = encode_map(&w, "value", 3);
		{
			w = encode_uint(&w, "status", status);
			w = encode_array(&w, "success_list", 7);
			w = mp_encode_uint(w, 200);
			w = mp_encode_uint(w, 201);
			w = mp_encode_uint(w, 202);
			w = mp_encode_uint(w, 203);
			w = mp_encode_uint(w, 204);
			w = mp_encode_uint(w, 205);
			w = mp_encode_uint(w, 206);
			w = encode_map(&w, "data", 1);
			{
				w = encode_str(&w, "tunnel_id", tunnel_id);
			}
		}
	}

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent REPLY with msg_uuid '%s'", msg_uuid);
	else
		log_debug("Failed to send REPLY");

	return result;
}

result_t send_route_request_reply(const node_t *node, char *msg_uuid, char *to_rt_uuid, uint32_t status, char *dest_peer_id)
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL;

	buffer = malloc(300);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return result;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "cmd", "REPLY");
		w = encode_map(&w, "value", 3);
		{
			w = encode_uint(&w, "status", status);
			w = encode_array(&w, "success_list", 7);
			w = mp_encode_uint(w, 200);
			w = mp_encode_uint(w, 201);
			w = mp_encode_uint(w, 202);
			w = mp_encode_uint(w, 203);
			w = mp_encode_uint(w, 204);
			w = mp_encode_uint(w, 205);
			w = mp_encode_uint(w, 206);
			w = encode_map(&w, "data", 1);
			{
				w = encode_str(&w, "peer_id", dest_peer_id);
			}
		}
	}

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent REPLY with msg_uuid '%s'", msg_uuid);
	else
		log_debug("Failed to send REPLY");

	return result;
}

result_t send_port_connect_reply(const node_t *node, char *msg_uuid, char *to_rt_uuid, uint32_t status, char *port_id)
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL;

	buffer = malloc(500);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "cmd", "REPLY");
		w = encode_map(&w, "value", 3);
		{
			w = encode_uint(&w, "status", status);
			w = encode_array(&w, "success_list", 7);
			w = mp_encode_uint(w, 200);
			w = mp_encode_uint(w, 201);
			w = mp_encode_uint(w, 202);
			w = mp_encode_uint(w, 203);
			w = mp_encode_uint(w, 204);
			w = mp_encode_uint(w, 205);
			w = mp_encode_uint(w, 206);
			w = encode_map(&w, "data", 1);
			{
				w = encode_str(&w, "port_id", port_id);
			}
		}
	}

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent REPLY with msg_uuid '%s'", msg_uuid);
	else
		log_debug("Failed to send REPLY");

	return result;
}

result_t send_token(const node_t *node, port_t *port, token_t *token)
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL;

	buffer = malloc(400);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", port->peer_id);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
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

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent TOKEN");
	else
		log_debug("Failed to send TOKEN");

	return result;
}

result_t send_token_reply(const node_t *node, port_t *port, uint32_t sequencenbr, char *status)
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL;

	buffer = malloc(400);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", port->peer_id);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
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

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent TOKEN_REPLY");
	else
		log_debug("Failed to send TOKEN_REPLY");

	return result;
}

result_t send_port_connect(const node_t *node, port_t *port, result_t (*handler)(char*, void*))
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL, *msg_uuid = NULL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, port) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	buffer = malloc(500);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 11);
	{
		w = encode_str(&w, "to_rt_uuid", port->peer_id);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
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

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent PORT_CONNECT with msg_uuid '%s'", msg_uuid);
	else {
		log_debug("Failed to send PORT_CONNECT");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_port_disconnect(const node_t *node, port_t *port, result_t (*handler)(char*, void*))
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL, *msg_uuid = NULL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, NULL) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	buffer = malloc(400);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 10);
	{
		w = encode_str(&w, "to_rt_uuid", port->peer_id);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = encode_str(&w, "tunnel_id", port->tunnel->tunnel_id);
		w = encode_str(&w, "cmd", "PORT_DISCONNECT");
		w = encode_nil(&w, "peer_port_name");
		w = encode_nil(&w, "peer_actor_id");
		w = encode_str(&w, "peer_port_id", port->peer_port_id);
		w = encode_str(&w, "port_id", port->port_id);
		w = encode_nil(&w, "peer_port_dir");
	}

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent PORT_DISCONNECT with msg_uuid '%s'", msg_uuid);
	else {
		log_debug("Failed to send PORT_DISCONNECT");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_remove_node(const node_t *node, result_t (*handler)(char*, void*))
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL, key[50] = "", *msg_uuid = NULL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, NULL) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	buffer = malloc(350);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	sprintf(key, "node-%s", node->node_id);

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id);
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

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent SET '%s' with msg_uuid '%s'", key, msg_uuid);
	else {
		log_debug("Failed to send SET '%s'", key);
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_set_actor(const node_t *node, const actor_t *actor, result_t (*handler)(char*, void*))
{
	int len = 0;
	char *buffer = NULL, *w = NULL, key[50] = "", data[400] = "", inports[100] = "", outports[100] = "", *msg_uuid = NULL;
	port_t *port = NULL;
	result_t result = FAIL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, NULL) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	// TODO: Set buffer size based on actor size
	buffer = malloc(600);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
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
				  node->node_id,
				  actor->type,
				  inports,
				  outports);

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id);
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

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent SET '%s' with msg_uuid '%s'", key, msg_uuid);
	else {
		log_debug("Failed to send SET '%s'", key);
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_remove_actor(const node_t *node, actor_t *actor, result_t (*handler)(char*, void*))
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL, key[50], *msg_uuid = NULL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, NULL) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	buffer = malloc(350);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	sprintf(key, "actor-%s", actor->id);

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id);
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

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent SET '%s' with msg_uuid '%s'", key, msg_uuid);
	else {
		log_debug("Failed to send SET '%s'", key);
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_set_port(const node_t *node, port_t *port, result_t (*handler)(char*, void*))
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL, key[50] = "", data[400] = "", *msg_uuid = NULL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, NULL) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	buffer = malloc(600);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	sprintf(key, "port-%s", port->port_id);
	sprintf(data,
			"{\"properties\": {\"direction\": \"%s\", \"routing\": \"default\", \"nbr_peers\": 1}, \"name\": \"%s\", \"node_id\": \"%s\", \"connected\": %s, \"actor_id\": \"%s\"}",
			port->direction == IN ? STRING_IN : STRING_OUT,
			port->port_name,
			node->node_id,
			port->state == PORT_CONNECTED ? STRING_TRUE : STRING_FALSE,
			port->actor->id);

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id);
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

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent SET '%s' with msg_uuid '%s'", key, msg_uuid);
	else {
		log_debug("Failed to send SET '%s'", key);
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_get_port(const node_t *node, char *port_id, result_t (*handler)(char*, void*), void *msg_data)
{
	result_t result = FAIL;
	char *msg_uuid = NULL, *buffer = NULL, *w = NULL, key[50] = "";

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, msg_data) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	buffer = malloc(350);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	sprintf(key, "port-%s", port_id);

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "cmd", "TUNNEL_DATA");
		w = encode_str(&w, "tunnel_id", node->storage_tunnel->tunnel_id);
		w = encode_map(&w, "value", 3);
		{
			w = encode_str(&w, "cmd", "GET");
			w = encode_str(&w, "key", key);
			w = encode_str(&w, "msg_uuid", msg_uuid);
		}
	}

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent GET '%s' with msg_uuid '%s'", key, msg_uuid);
	else {
		log_debug("Failed to send GET '%s'", key);
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_remove_port(const node_t *node, port_t *port, result_t (*handler)(char*, void*))
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL, key[50] = "", *msg_uuid = NULL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, NULL) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	buffer = malloc(350);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	sprintf(key, "port-%s", port->port_id);

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id);
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

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent SET '%s' with msg_uuid '%s'", key, msg_uuid);
	else {
		log_debug("Failed to send SET '%s'", key);
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

result_t send_actor_new(const node_t *node, actor_t *actor, char *to_rt_uuid, result_t (*handler)(char*, void*))
{
	result_t result = FAIL;
	char *buffer = NULL, *w = NULL, *msg_uuid = NULL;

	msg_uuid = gen_uuid("MSGID_");
	if (msg_uuid == NULL) {
		log_error("Failed to generate msg_uuid");
		return result;
	}

	if (add_pending_msg(msg_uuid, handler, NULL) != SUCCESS) {
		free(msg_uuid);
		return result;
	}

	// TODO: Set buffer size based on actor size
	// 1800 is from serializing the identity actor
	buffer = malloc(1800);
	if (buffer == NULL) {
		log_error("Failed to allocate memory");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
		return result;
	}

	w = buffer + 4;

	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid);
		w = encode_str(&w, "from_rt_uuid", node->node_id);
		w = encode_str(&w, "cmd", "ACTOR_NEW");
		w = encode_str(&w, "msg_uuid", msg_uuid);
		w = serialize_actor(actor, &w);
	}

	result = client_send(node->transport, buffer, w - buffer - 4);
	if (result == SUCCESS)
		log_debug("Sent ACTOR_NEW with msg_uuid '%s'", msg_uuid);
	else {
		log_debug("Failed to send ACTOR_NEW");
		remove_pending_msg(msg_uuid);
		free(msg_uuid);
	}

	return result;
}

static result_t parse_reply(node_t *node, char *root)
{
	result_t result = FAIL;
	char *msg_uuid = NULL, *r = root;
	pending_msg_t *pending_msgs = node->pending_msgs;

	if (decode_string_from_map(&r, "msg_uuid", &msg_uuid) == SUCCESS) {
		while (pending_msgs != NULL) {
			if (strcmp(pending_msgs->msg_uuid, msg_uuid) == 0) {
				if (pending_msgs->handler != NULL) {
					result = pending_msgs->handler(root, pending_msgs->msg_data);
					remove_pending_msg(msg_uuid);
					free(msg_uuid);
					return result;
				}
				log_error("Unknown message %s", msg_uuid);
				remove_pending_msg(msg_uuid);
				break;
			}
			pending_msgs = pending_msgs->next;
		}
		free(msg_uuid);
	}

	return result;
}

static result_t parse_token(node_t *node, char *root)
{
	result_t result = FAIL;
	char *obj_value = NULL, *obj_token = NULL, *from_rt_uuid = NULL, *port_id = NULL, *r = root;
	uint32_t sequencenbr = 0;
	token_t *token = NULL;

	result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);

	if (result == SUCCESS)
		result = get_value_from_map(&r, "value", &obj_value);

	if (result == SUCCESS)
		result = decode_string_from_map(&obj_value, "peer_port_id", &port_id);

	if (result == SUCCESS)
		result = decode_uint_from_map(&obj_value, "sequencenbr", &sequencenbr);

	if (result == SUCCESS)
		result = get_value_from_map(&obj_value, "token", &obj_token);

	if (result != SUCCESS)
		log_error("Failed to decode message");

	if (result == SUCCESS) {
		if (decode_token(obj_token, &token) == SUCCESS)
			result = handle_token(port_id, token, sequencenbr);
		else
			log_error("Failed to create token");
	}

	if (port_id != NULL)
		free(port_id);

	if (from_rt_uuid != NULL)
		free(from_rt_uuid);

	return result;
}

static result_t parse_token_reply(node_t *node, char *root)
{
	result_t result = FAIL;
	char *value = NULL, *r = root, *port_id = NULL, *status = NULL;
	uint32_t sequencenbr = 0;

	if (get_value_from_map(&r, "value", &value) == SUCCESS) {
		if (decode_string_from_map(&value, "port_id", &port_id) == SUCCESS) {
			if (decode_string_from_map(&value, "value", &status) == SUCCESS) {
				if (decode_uint_from_map(&value, "sequencenbr", &sequencenbr) == SUCCESS) {
					if (strcmp(status, "ACK") == 0) {
						handle_token_reply(port_id, PORT_REPLY_TYPE_ACK);
						result = SUCCESS;
					} else if (strcmp(status, "NACK") == 0) {
						handle_token_reply(port_id, PORT_REPLY_TYPE_NACK);
						result = SUCCESS;
					} else if (strcmp(status, "ABORT") == 0) {
						handle_token_reply(port_id, PORT_REPLY_TYPE_ABORT);
						result = SUCCESS;
					} else {
						log_error("Received unknown token reply '%s' for token with sequencenbr '%ld' on port '%s'",
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

static result_t parse_tunnel_data(node_t *node, char *root)
{
	char *msg_uuid = NULL, *value = NULL, *cmd = NULL, *r = root;
	pending_msg_t *pending_msgs = node->pending_msgs;
	result_t result = FAIL;

	result = get_value_from_map(&r, "value", &value);
	if (result == SUCCESS) {
		if (has_key(&value, "msg_uuid")) {
			result = decode_string_from_map(&value, "msg_uuid", &msg_uuid);
			while (result == SUCCESS && pending_msgs != NULL) {
				if (strcmp(pending_msgs->msg_uuid, msg_uuid) == 0) {
					if (pending_msgs->handler != NULL) {
						log_debug("Calling handler for msg_uuid '%s'", msg_uuid);
						result = pending_msgs->handler(root, pending_msgs->msg_data);
					} else {
						log_error("Received message without a handler set %s", msg_uuid);
						result = FAIL;
					}
					remove_pending_msg(msg_uuid);
					free(msg_uuid);
					return result;
				}
				pending_msgs = pending_msgs->next;
			}
			free(msg_uuid);
		} else if (has_key(&value, "cmd")) {
			if (decode_string_from_map(&value, "cmd", &cmd) == SUCCESS) {
				log_debug("Received tunneled command '%s'", cmd);
				if (strcmp(cmd, "TOKEN") == 0)
					result = parse_token(node, root);
				else if (strcmp(cmd, "TOKEN_REPLY") == 0)
					result = parse_token_reply(node, root);
				else
					log_error("Unhandled tunnel cmd '%s'", cmd);
				free(cmd);
				return result;
			}
		} else
			log_error("Unknown message");
	}

	return FAIL;
}

static result_t parse_actor_new(node_t *node, char *root)
{
	result_t result = SUCCESS;
	actor_t *actor = NULL;
	char *from_rt_uuid = NULL, *msg_uuid = NULL, *r = root;

	if (decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid) == SUCCESS) {
		if (decode_string_from_map(&r, "msg_uuid", &msg_uuid) == SUCCESS) {
			if (create_actor(node, root, &actor) != SUCCESS) {
				log_error("Failed to create actor");
				result = FAIL;
			}
		}
	}

	if (result == SUCCESS)
		result = send_reply(node, msg_uuid, from_rt_uuid, 200);
	else {
		result = send_reply(node, msg_uuid, from_rt_uuid, 500);
		if (actor != NULL)
			delete_actor(node, actor, true);
	}

	if (from_rt_uuid != NULL)
		free(from_rt_uuid);

	if (msg_uuid != NULL)
		free(msg_uuid);

	return result;
}

static result_t actor_new_reply_handler(char *data, void *msg_data)
{
	log_debug("TODO: actor_new_reply_handler does nothing");
	return SUCCESS;
}

static result_t parse_actor_migrate(node_t *node, char *root)
{
	result_t result = SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *to_rt_uuid = NULL, *actor_id = NULL, *msg_uuid = NULL;
	actor_t *actor = NULL;

	result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "to_rt_uuid", &to_rt_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "actor_id", &actor_id);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);

	if (result == SUCCESS) {
		actor = get_actor(node, actor_id);
		if (actor != NULL) {
			if (disconnect_actor(node, actor) == SUCCESS) {
				result = send_actor_new(node, actor, from_rt_uuid, actor_new_reply_handler);
				if (result != SUCCESS)
					log_error("Failed to migrate actor");
			} else
				log_error("Failed to disconnect actor '%s'", actor->id);
		} else {
			log_error("No actor with id '%s'", actor_id);
			result = FAIL;
		}
	}

	if (result == SUCCESS)
		result = send_reply(node, msg_uuid, from_rt_uuid, 200);
	else
		result = send_reply(node, msg_uuid, from_rt_uuid, 500);

	if (from_rt_uuid != NULL)
		free(from_rt_uuid);

	if (to_rt_uuid != NULL)
		free(to_rt_uuid);

	if (actor_id != NULL)
		free(actor_id);

	if (msg_uuid != NULL)
		free(msg_uuid);

	return result;
}

static result_t parse_app_destroy(node_t *node, char *root)
{
	result_t result = SUCCESS;

	char *r = root, *from_rt_uuid = NULL, *to_rt_uuid = NULL, *msg_uuid = NULL;
	char *obj_actor_uuids = NULL, *actor_id = NULL;
	uint32_t i = 0, size = 0;
	actor_t *actor = NULL;

	result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "to_rt_uuid", &to_rt_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);

	if (result == SUCCESS) {
		result = get_value_from_map(&r, "actor_uuids", &obj_actor_uuids);
		if (result == SUCCESS) {
			size = mp_decode_array((const char **)&obj_actor_uuids);
			for (i = 0; i < size; i++) {
				result = decode_str(&obj_actor_uuids, &actor_id);
				if (result == SUCCESS) {
					actor = get_actor(node, actor_id);
					delete_actor(node, actor, true);
					free(actor_id);
				}
			}
		}
	}

	if (result == SUCCESS)
		result = send_reply(node, msg_uuid, from_rt_uuid, 200);
	else
		result = send_reply(node, msg_uuid, from_rt_uuid, 500);

	if (from_rt_uuid != NULL)
		free(from_rt_uuid);

	if (to_rt_uuid != NULL)
		free(to_rt_uuid);

	if (msg_uuid != NULL)
		free(msg_uuid);

	return result;
}

static result_t parse_port_disconnect(node_t *node, char *root)
{
	result_t result = SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *to_rt_uuid = NULL, *msg_uuid = NULL, *peer_port_id = NULL;

	result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "to_rt_uuid", &to_rt_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "peer_port_id", &peer_port_id);

	if (result == SUCCESS)
		result = handle_port_disconnect(node, peer_port_id);

	if (result == SUCCESS)
		result = send_reply(node, msg_uuid, from_rt_uuid, 200);
	else
		result = send_reply(node, msg_uuid, from_rt_uuid, 500);

	if (from_rt_uuid != NULL)
		free(from_rt_uuid);

	if (to_rt_uuid != NULL)
		free(to_rt_uuid);

	if (msg_uuid != NULL)
		free(msg_uuid);

	if (peer_port_id != NULL)
		free(peer_port_id);

	return result;
}

static result_t parse_port_connect(node_t *node, char *root)
{
	result_t result = SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *to_rt_uuid = NULL, *msg_uuid = NULL;
	char *port_id = NULL, *peer_port_id = NULL, *tunnel_id = NULL;

	result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "to_rt_uuid", &to_rt_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "port_id", &port_id);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "peer_port_id", &peer_port_id);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "tunnel_id", &tunnel_id);

	if (result == SUCCESS)
		result = handle_port_connect(node, peer_port_id, tunnel_id);

	if (result == SUCCESS)
		result = send_port_connect_reply(node, msg_uuid, from_rt_uuid, 200, peer_port_id);
	else
		result = send_port_connect_reply(node, msg_uuid, from_rt_uuid, 404, peer_port_id);

	if (tunnel_id != NULL)
		free(tunnel_id);

	if (from_rt_uuid != NULL)
		free(from_rt_uuid);

	if (to_rt_uuid != NULL)
		free(to_rt_uuid);

	if (msg_uuid != NULL)
		free(msg_uuid);

	if (port_id != NULL)
		free(port_id);

	if (peer_port_id != NULL)
		free(peer_port_id);

	return result;
}

static result_t parse_tunnel_new(node_t *node, char *root)
{
	result_t result = SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *msg_uuid = NULL;
	char *type = NULL, *tunnel_id;

	result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);

	if (result == SUCCESS) {
		result = decode_string_from_map(&r, "type", &type);
		if (result == SUCCESS && strcmp(type, "token") != 0) {
			log_error("Only token tunnels supported");
			result = FAIL;
		}
	}

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "tunnel_id", &tunnel_id);

	if (result == SUCCESS)
		result = handle_tunnel_new_request(node, from_rt_uuid, tunnel_id);

	if (result == SUCCESS) {
		result = send_tunnel_new_reply(node, msg_uuid, from_rt_uuid, 200, tunnel_id);
		tunnel_connected(node, get_tunnel(node, tunnel_id));
	} else
		result = send_tunnel_new_reply(node, msg_uuid, from_rt_uuid, 404, tunnel_id);

	if (from_rt_uuid != NULL)
		free(from_rt_uuid);

	if (msg_uuid != NULL)
		free(msg_uuid);

	if (type != NULL)
		free(type);

	if (tunnel_id != NULL)
		free(tunnel_id);

	return result;
}

static result_t parse_route_request(node_t *node, char *root)
{
	result_t result = SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *msg_uuid = NULL;
	char *dest_peer_id = NULL, *org_peer_id = NULL;
	link_t *link = NULL;

	result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "dest_peer_id", &dest_peer_id);

	if (result == SUCCESS)
		result = decode_string_from_map(&r, "org_peer_id", &org_peer_id);

	if (result != SUCCESS)
		log_error("Failed to decode message");

	if (result == SUCCESS) {
		if (strcmp(dest_peer_id, node->node_id) == 0) {
			link = get_link(node, org_peer_id);
			if (link != NULL)
				log_debug("Link to '%s' already exists", dest_peer_id);
			else {
				link = create_link(org_peer_id, LINK_WORKING);
				result = add_link(node, link);
			}
		} else {
			log_error("Routing not supported");
			result = FAIL;
		}
	}

	if (result == SUCCESS)
		result = send_route_request_reply(node, msg_uuid, org_peer_id, 200, dest_peer_id);
	else
		result = send_route_request_reply(node, msg_uuid, org_peer_id, 501, dest_peer_id);

	if (from_rt_uuid != NULL)
		free(from_rt_uuid);

	if (msg_uuid != NULL)
		free(msg_uuid);

	if (dest_peer_id != NULL)
		free(dest_peer_id);

	if (org_peer_id != NULL)
		free(org_peer_id);

	return result;
}

result_t parse_message(node_t *node, char *data)
{
	result_t result = SUCCESS;
	char *cmd = NULL, *r = data, *msg_uuid = NULL, *from_rt_uuid = NULL;
	int i = 0;

	result = decode_string_from_map(&r, "cmd", &cmd);
	if (result == SUCCESS) {
		log_debug("Received '%s'", cmd);
		for (i = 0; i < NBR_OF_COMMANDS; i++) {
			if (strcmp(cmd, command_handlers[i].command) == 0) {
				result = command_handlers[i].handler(node, r);
				free(cmd);
				return result;
			}
		}

		log_error("Unhandled command '%s'", cmd);
		result = decode_string_from_map(&r, "msg_uuid", &msg_uuid);
		if (result == SUCCESS) {
			result = decode_string_from_map(&r, "from_rt_uuid", &from_rt_uuid);
			if (result == SUCCESS) {
				result = send_reply(node, msg_uuid, from_rt_uuid, 500);
				free(from_rt_uuid);
			}
			free(msg_uuid);
		}
		result = FAIL;
		free(cmd);
	} else
		log_error("Failed to decode 'cmd'");

	return result;
}
