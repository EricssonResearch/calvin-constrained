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
#include "cc_proto.h"
#include "cc_msgpack_helper.h"
#include "cc_transport.h"
#include "cc_link.h"
#include "../south/platform/cc_platform.h"
#include "../../msgpuck/msgpuck.h"

#define STRING_TRUE			"true"
#define STRING_FALSE		"false"
#define STRING_IN				"in"
#define STRING_OUT			"out"
#define NBR_OF_COMMANDS	9
#define SERIALIZER			"msgpack"

struct command_handler_t {
	char command[50];
	result_t (*handler)(node_t *node, char *data);
};

static result_t proto_parse_reply(node_t *node, char *data);
static result_t proto_parse_tunnel_data(node_t *node, char *data);
static result_t proto_parse_actor_new(node_t *node, char *data);
static result_t proto_parse_app_destroy(node_t *node, char *data);
static result_t proto_parse_port_disconnect(node_t *node, char *data);
static result_t proto_parse_port_connect(node_t *node, char *data);
static result_t proto_parse_tunnel_new(node_t *node, char *data);
static result_t proto_parse_actor_migrate(node_t *node, char *root);

struct command_handler_t command_handlers[NBR_OF_COMMANDS] = {
	{"REPLY", proto_parse_reply},
	{"TUNNEL_DATA", proto_parse_tunnel_data},
	{"ACTOR_NEW", proto_parse_actor_new},
	{"APP_DESTROY", proto_parse_app_destroy},
	{"PORT_DISCONNECT", proto_parse_port_disconnect},
	{"PORT_CONNECT", proto_parse_port_connect},
	{"TUNNEL_NEW", proto_parse_tunnel_new},
	{"ACTOR_MIGRATE", proto_parse_actor_migrate}
};

result_t proto_send_join_request(const node_t *node, transport_client_t *transport_client, const char *serializer)
{
	char buffer[600], *w = NULL, msg_uuid[UUID_BUFFER_SIZE];
	int size = 0;

	gen_uuid(msg_uuid, "MSGID_");

	w = buffer + transport_client->prefix_len;
	size = sprintf(w,
		"{\"cmd\": \"JOIN_REQUEST\", \"id\": \"%s\", \"sid\": \"%s\", \"serializers\": [\"%s\"]}",
		node->id,
		msg_uuid,
		serializer);

	if (transport_send(transport_client, buffer, size + transport_client->prefix_len) == CC_RESULT_SUCCESS)
		return CC_RESULT_SUCCESS;

	log_error("Failed to send JOIN_REQUEST");

	return CC_RESULT_FAIL;
}

result_t proto_send_node_setup(node_t *node, result_t (*handler)(node_t*, char*, void*))
{
	char buffer[1000], *w = NULL, msg_uuid[UUID_BUFFER_SIZE], name[] = "name";
	list_t *calvinsys = node->calvinsys;
	uint32_t nbr_of_capabilities = 0;
	int map_count = 0;
	list_t *list;

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	gen_uuid(msg_uuid, "MSGID_");

	while (calvinsys != NULL) {
		nbr_of_capabilities++;
		calvinsys = calvinsys->next;
	}

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, node->attributes == NULL ? 8 : 9);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid, strlen(msg_uuid));
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id, strlen(node->proxy_link->peer_id));
		w = encode_str(&w, "cmd", "PROXY_CONFIG", strlen("PROXY_CONFIG"));
		w = encode_str(&w, "name", node->name, strlen(node->name));
		w = encode_array(&w, "capabilities", nbr_of_capabilities);
		{
			calvinsys = node->calvinsys;
			while (calvinsys != NULL) {
				log("Setting capability %s", calvinsys->id);
				w = mp_encode_str(w, calvinsys->id, strlen(calvinsys->id));
				calvinsys = calvinsys->next;
			}
		}
		w = encode_str(&w, "port_property_capability", "runtime.constrained.1", strlen("runtime.constrained.1"));
		w = encode_bool(&w, "redeploy", 0);

		if (node->attributes != NULL) {
			if (list_get(node->attributes->indexed_public_node_name, "name") == NULL)
				list_add_n(&(node->attributes)->indexed_public_node_name, name, strlen(name), node->name, strlen(node->name));
			if (node->attributes->indexed_public_owner != NULL)
				map_count++;
			if (node->attributes->indexed_public_address != NULL)
				map_count++;
			if (node->attributes->indexed_public_node_name != NULL)
				map_count++;
			w = encode_map(&w, "attributes", 1);
			w = encode_map(&w, "indexed_public", map_count);

			if (node->attributes->indexed_public_owner != NULL) {
				map_count = list_count(node->attributes->indexed_public_owner);
				w = encode_map(&w, "public_owner", map_count);
				list = node->attributes->indexed_public_owner;
				while (list != NULL) {
				  w = encode_str(&w, list->id, (const char *)list->data, list->data_len);
				  list = list->next;
				}
			}
			if (node->attributes->indexed_public_address != NULL) {
				map_count = list_count(node->attributes->indexed_public_address);
				w = encode_map(&w, "address", map_count);
				list = node->attributes->indexed_public_address;
				while (list != NULL) {
					w = encode_str(&w, list->id, (const char *)list->data, list->data_len);
					list = list->next;
				}
			}
			if (node->attributes->indexed_public_node_name != NULL) {
				map_count = list_count(node->attributes->indexed_public_node_name);
				w = encode_map(&w, "node_name", map_count);
				list = node->attributes->indexed_public_node_name;
				while (list != NULL) {
				  w = encode_str(&w, list->id, (const char *)list->data, list->data_len);
				  list = list->next;
				}
			}
		}
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent PROXY_CONFIG");
		node_add_pending_msg(node, msg_uuid, strlen(msg_uuid), handler, NULL);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_tunnel_request(node_t *node, tunnel_t *tunnel, result_t (*handler)(node_t*, char*, void*))
{
	char buffer[1000], *w = NULL, msg_uuid[UUID_BUFFER_SIZE];

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 7);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid, strlen(msg_uuid));
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "to_rt_uuid", tunnel->link->peer_id, strlen(tunnel->link->peer_id));
		w = encode_str(&w, "cmd", "TUNNEL_NEW", strlen("TUNNEL_NEW"));
		w = encode_str(&w, "tunnel_id", tunnel->id, strlen(tunnel->id));
		w = encode_str(&w, "type", tunnel->type == TUNNEL_TYPE_STORAGE ? STORAGE_TUNNEL : TOKEN_TUNNEL, tunnel->type == TUNNEL_TYPE_STORAGE ? strlen(STORAGE_TUNNEL) : strlen(TOKEN_TUNNEL));
		w = encode_map(&w, "policy", 0);
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		node_add_pending_msg(node, msg_uuid, strlen(msg_uuid), handler, tunnel);
		log_debug("Sent TUNNEL_NEW with id '%s' to '%s'", tunnel->id, tunnel->link->peer_id);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_tunnel_destroy(node_t *node, tunnel_t *tunnel, result_t (*handler)(node_t*, char*, void*))
{
	char buffer[1000], *w = NULL, msg_uuid[UUID_BUFFER_SIZE];

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid, strlen(msg_uuid));
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "to_rt_uuid", tunnel->link->peer_id, strlen(tunnel->link->peer_id));
		w = encode_str(&w, "cmd", "TUNNEL_DESTROY", strlen("TUNNEL_DESTROY"));
		w = encode_str(&w, "tunnel_id", tunnel->id, strlen(tunnel->id));
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		node_add_pending_msg(node, msg_uuid, strlen(msg_uuid), handler, tunnel->id);
		log_debug("Sent TUNNEL_DESTROY for tunnel '%s'", tunnel->id);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_reply(const node_t *node, char *msg_uuid, uint32_t msg_uuid_len, char *to_rt_uuid, uint32_t to_rt_uuid_len, uint32_t status)
{
	char buffer[1000], *w = NULL;

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid, msg_uuid_len);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "cmd", "REPLY", strlen("REPLY"));
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

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent REPLY for message '%.*s'", (int)msg_uuid_len, msg_uuid);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_tunnel_new_reply(const node_t *node, char *msg_uuid, uint32_t msg_uuid_len, char *to_rt_uuid, uint32_t to_rt_uuid_len, uint32_t status, char *tunnel_id, uint32_t tunnel_id_len)
{
	char buffer[1000], *w = NULL;

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid, msg_uuid_len);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "cmd", "REPLY", strlen("REPLY"));
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
				w = encode_str(&w, "tunnel_id", tunnel_id, tunnel_id_len);
			}
		}

		if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
			log_debug("Sent REPLY for message '%.*s'", (int)msg_uuid_len, msg_uuid);
			return CC_RESULT_SUCCESS;
		}
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_route_request_reply(const node_t *node, char *msg_uuid, uint32_t msg_uuid_len, char *to_rt_uuid, uint32_t to_rt_uuid_len, uint32_t status, char *dest_peer_id, uint32_t dest_peer_id_len)
{
	char buffer[1000], *w = NULL;

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid, msg_uuid_len);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "cmd", "REPLY", strlen("REPLY"));
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
				w = encode_str(&w, "peer_id", dest_peer_id, dest_peer_id_len);
			}
		}
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent REPLY for message '%.*s'", (int)msg_uuid_len, msg_uuid);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_port_connect_reply(const node_t *node, char *msg_uuid, uint32_t msg_uuid_len, char *to_rt_uuid, uint32_t to_rt_uuid_len, uint32_t status, char *port_id, uint32_t port_id_len)
{
	char buffer[1000], *w = NULL;

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid, msg_uuid_len);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "cmd", "REPLY", strlen("REPLY"));
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
				w = encode_str(&w, "port_id", port_id, port_id_len);
			}
		}
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent REPLY reply for message '%.*s'", (int)msg_uuid_len, msg_uuid);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_port_disconnect_reply(const node_t *node, char *msg_uuid, uint32_t msg_uuid_len, char *to_rt_uuid, uint32_t to_rt_uuid_len, uint32_t status)
{
	char buffer[1000], *w = NULL;

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "msg_uuid", msg_uuid, msg_uuid_len);
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "cmd", "REPLY", strlen("REPLY"));
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
				// TODO: If any, include remaining tokens
				w = encode_map(&w, "remaining_tokens", 0);
			}
		}
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent REPLY for message '%.*s'", (int)msg_uuid_len, msg_uuid);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_token(const node_t *node, port_t *port, token_t token, uint32_t sequencenbr)
{
	char buffer[1000], *w = NULL, *peer_id = NULL;

	peer_id = port_get_peer_id(node, port);
	if (peer_id == NULL)
		return CC_RESULT_FAIL;

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", peer_id, strlen(peer_id));
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "cmd", "TUNNEL_DATA", strlen("TUNNEL_DATA"));
		w = encode_str(&w, "tunnel_id", port->tunnel->id, strlen(port->tunnel->id));
		w = encode_map(&w, "value", 5);
		{
			w = encode_str(&w, "cmd", "TOKEN", strlen("TOKEN"));
			w = encode_uint(&w, "sequencenbr", sequencenbr);
			w = encode_str(&w, "port_id", port->id, strlen(port->id));
			w = encode_str(&w, "peer_port_id", port->peer_port_id, strlen(port->peer_port_id));
			w = token_encode(&w, token, true);
		}
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent TOKEN with sequencenbr '%ld'", (unsigned long)sequencenbr);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_token_reply(const node_t *node, port_t *port, uint32_t sequencenbr, bool ack)
{
	char buffer[1000], *w = NULL, *peer_id = NULL;

	peer_id = port_get_peer_id(node, port);
	if (peer_id == NULL) {
		log_error("Port has no peer");
		return CC_RESULT_FAIL;
	}

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", peer_id, strlen(peer_id));
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "cmd", "TUNNEL_DATA", strlen("TUNNEL_DATA"));
		w = encode_str(&w, "tunnel_id", port->tunnel->id, strlen(port->tunnel->id));
		w = encode_map(&w, "value", 5);
		{
			w = encode_str(&w, "cmd", "TOKEN_REPLY", strlen("TOKEN_REPLY"));
			w = encode_uint(&w, "sequencenbr", sequencenbr);
			w = encode_str(&w, "peer_port_id", port->id, strlen(port->id));
			w = encode_str(&w, "port_id", port->peer_port_id, strlen(port->peer_port_id));
			w = encode_str(&w, "value", ack ? "ACK" : "NACK", strlen(ack ? "ACK" : "NACK"));
		}
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent TOKEN_REPLY for sequencenbr '%ld'", (unsigned long)sequencenbr);
		return CC_RESULT_SUCCESS;
	}

	log_error("Failed to send TOKEN_REPLY");

	return CC_RESULT_FAIL;
}

result_t proto_send_port_connect(node_t *node, port_t *port, result_t (*handler)(node_t*, char*, void*))
{
	char buffer[1000], *w = NULL, msg_uuid[UUID_BUFFER_SIZE], *peer_id = NULL;

	peer_id = port_get_peer_id(node, port);
	if (peer_id == NULL)
		return CC_RESULT_FAIL;

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 11);
	{
		w = encode_str(&w, "to_rt_uuid", peer_id, strlen(peer_id));
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "msg_uuid", msg_uuid, strlen(msg_uuid));
		w = encode_str(&w, "tunnel_id", port->tunnel->id, strlen(port->tunnel->id));
		w = encode_str(&w, "cmd", "PORT_CONNECT", strlen("PORT_CONNECT"));
		w = encode_nil(&w, "peer_port_name");
		w = encode_nil(&w, "peer_actor_id");
		w = encode_str(&w, "peer_port_id", port->peer_port_id, strlen(port->peer_port_id));
		w = encode_str(&w, "port_id", port->id, strlen(port->id));
		w = encode_nil(&w, "peer_port_properties");
		w = encode_map(&w, "port_properties", 3);
		w = encode_str(&w, "direction", port->direction == PORT_DIRECTION_IN ? STRING_IN : STRING_OUT, strlen(port->direction == PORT_DIRECTION_IN ? STRING_IN : STRING_OUT));
		w = encode_str(&w, "routing", "default", strlen("default"));
		w = encode_uint(&w, "nbr_peers", 1);
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent PORT_CONNECT for port '%s'", port->id);
		node_add_pending_msg(node, msg_uuid, strlen(msg_uuid), handler, port->id);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_port_disconnect(node_t *node, port_t *port, result_t (*handler)(node_t*, char*, void*))
{
	char buffer[1000], *w = NULL, msg_uuid[UUID_BUFFER_SIZE], *peer_id = NULL;

	peer_id = port_get_peer_id(node, port);
	if (peer_id == NULL)
		return CC_RESULT_FAIL;

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 10);
	{
		w = encode_str(&w, "to_rt_uuid", peer_id, strlen(peer_id));
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "msg_uuid", msg_uuid, strlen(msg_uuid));
		w = encode_str(&w, "tunnel_id", port->tunnel->id, strlen(port->tunnel->id));
		w = encode_str(&w, "cmd", "PORT_DISCONNECT", strlen("PORT_DISCONNECT"));
		w = encode_nil(&w, "peer_port_name");
		w = encode_nil(&w, "peer_actor_id");
		w = encode_str(&w, "peer_port_id", port->peer_port_id, strlen(port->peer_port_id));
		w = encode_str(&w, "port_id", port->id, strlen(port->id));
		w = encode_nil(&w, "peer_port_dir");
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		node_add_pending_msg(node, msg_uuid, strlen(msg_uuid), handler, port->id);
		log_debug("Sent PORT_DISCONNECT for port '%s'", port->id);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_set_actor(node_t *node, const actor_t *actor, result_t (*handler)(node_t*, char*, void*))
{
	int data_len = 0, inports_len = 0, outports_len = 0;
	char buffer[1000], *w = NULL, key[50] = "", data[400] = "", inports[200] = "", outports[200] = "", msg_uuid[UUID_BUFFER_SIZE];
	list_t *list = NULL;
	port_t *port = NULL;

	sprintf(key, "actor-%s", actor->id);

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	gen_uuid(msg_uuid, "MSGID_");

	list = actor->in_ports;
	while (list != NULL) {
		port = (port_t *)list->data;
		if (list->next != NULL)
			inports_len += sprintf(inports + inports_len,
						   "{\"id\": \"%s\", \"name\": \"%s\"}, ",
						   port->id,
						   port->name);
		else
			inports_len += sprintf(inports + inports_len,
						   "{\"id\": \"%s\", \"name\": \"%s\"}",
						   port->id,
						   port->name);
		list = list->next;
	}

	list = actor->out_ports;
	while (list != NULL) {
		port = (port_t *)list->data;
		if (list->next != NULL)
			outports_len += sprintf(outports + outports_len,
						   "{\"id\": \"%s\", \"name\": \"%s\"}, ",
							port->id,
							port->name);
		else
			outports_len += sprintf(outports + outports_len,
						   "{\"id\": \"%s\", \"name\": \"%s\"}",
							port->id,
							port->name);
		list = list->next;
	}

	data_len = sprintf(data,
				  "{\"is_shadow\": false, \"name\": \"%s\", \"node_id\": \"%s\", \"type\": \"%s\", \"inports\": [%s], \"outports\": [%s]}",
				  actor->name,
				  node->id,
				  actor->type,
				  inports,
				  outports);

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id, strlen(node->proxy_link->peer_id));
		w = encode_str(&w, "cmd", "TUNNEL_DATA", strlen("TUNNEL_DATA"));
		w = encode_str(&w, "tunnel_id", node->storage_tunnel->id, strlen(node->storage_tunnel->id));
		w = encode_map(&w, "value", 4);
		{
			w = encode_str(&w, "cmd", "SET", strlen("SET"));
			w = encode_str(&w, "key", key, strlen(key));
			w = encode_str(&w, "value", data, data_len);
			w = encode_str(&w, "msg_uuid", msg_uuid, strlen(msg_uuid));
		}
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent SET '%s'", key);
		node_add_pending_msg(node, msg_uuid, strlen(msg_uuid), handler, (void *)actor->id);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_remove_actor(node_t *node, actor_t *actor, result_t (*handler)(node_t*, char*, void*))
{
	char buffer[1000], *w = NULL, key[50], msg_uuid[UUID_BUFFER_SIZE];

	sprintf(key, "actor-%s", actor->id);

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id, strlen(node->proxy_link->peer_id));
		w = encode_str(&w, "cmd", "TUNNEL_DATA", strlen("TUNNEL_DATA"));
		w = encode_str(&w, "tunnel_id", node->storage_tunnel->id, strlen(node->storage_tunnel->id));
		w = encode_map(&w, "value", 4);
		{
			w = encode_str(&w, "cmd", "SET", strlen("SET"));
			w = encode_str(&w, "key", key, strlen(key));
			w = encode_nil(&w, "value");
			w = encode_str(&w, "msg_uuid", msg_uuid, strlen(msg_uuid));
		}
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		node_add_pending_msg(node, msg_uuid, strlen(msg_uuid), handler, NULL);
		log_debug("Sent SET '%s' with nil", key);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_set_port(node_t *node, port_t *port, result_t (*handler)(node_t*, char*, void*))
{
	char buffer[2000], *w = NULL, key[50] = "", data[1000] = "", msg_uuid[UUID_BUFFER_SIZE], *peer_id = NULL;

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	gen_uuid(msg_uuid, "MSGID_");

	peer_id = port_get_peer_id(node, port);

	if (peer_id == NULL)
		peer_id = "null";

	sprintf(key, "port-%s", port->id);

	sprintf(data,
		"{\"peers\": [[\"%s\", \"%s\"]], \"properties\": {\"direction\": \"%s\", \"routing\": \"default\", \"nbr_peers\": 1}, \"name\": \"%s\", \"node_id\": \"%s\", \"connected\": %s, \"actor_id\": \"%s\"}",
		peer_id,
		port->peer_port_id,
		port->direction == PORT_DIRECTION_IN ? STRING_IN : STRING_OUT,
		port->name,
		node->id,
		STRING_TRUE,
		port->actor->id);

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id, strlen(node->proxy_link->peer_id));
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "cmd", "TUNNEL_DATA", strlen("TUNNEL_DATA"));
		w = encode_str(&w, "tunnel_id", node->storage_tunnel->id, strlen(node->storage_tunnel->id));
		w = encode_map(&w, "value", 4);
		{
			w = encode_str(&w, "cmd", "SET", strlen("SET"));
			w = encode_str(&w, "key", key, strlen(key));
			w = encode_str(&w, "value", data, strlen(data));
			w = encode_str(&w, "msg_uuid", msg_uuid, strlen(msg_uuid));
		}
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent SET '%s'", key);
		node_add_pending_msg(node, msg_uuid, strlen(msg_uuid), handler, (void *)port->id);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_get_port(node_t *node, char *port_id, result_t (*handler)(node_t*, char*, void*), void *msg_data)
{
	char buffer[1000], msg_uuid[UUID_BUFFER_SIZE], *w = NULL, key[50] = "";

	sprintf(key, "port-%s", port_id);

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id, strlen(node->proxy_link->peer_id));
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "cmd", "TUNNEL_DATA", strlen("TUNNEL_DATA"));
		w = encode_str(&w, "tunnel_id", node->storage_tunnel->id, strlen(node->storage_tunnel->id));
		w = encode_map(&w, "value", 3);
		{
			w = encode_str(&w, "cmd", "GET", strlen("GET"));
			w = encode_str(&w, "key", key, strlen(key));
			w = encode_str(&w, "msg_uuid", msg_uuid, strlen(msg_uuid));
		}
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent GET '%s'", key);
		node_add_pending_msg(node, msg_uuid, strlen(msg_uuid), handler, msg_data);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_remove_port(node_t *node, port_t *port, result_t (*handler)(node_t*, char*, void*))
{
	char buffer[1000], *w = NULL, key[50] = "", msg_uuid[UUID_BUFFER_SIZE];

	sprintf(key, "port-%s", port->id);

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", node->proxy_link->peer_id, strlen(node->proxy_link->peer_id));
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "cmd", "TUNNEL_DATA", strlen("TUNNEL_DATA"));
		w = encode_str(&w, "tunnel_id", node->storage_tunnel->id, strlen(node->storage_tunnel->id));
		w = encode_map(&w, "value", 4);
		{
			w = encode_str(&w, "cmd", "SET", strlen("SET"));
			w = encode_str(&w, "key", key, strlen(key));
			w = encode_nil(&w, "value");
			w = encode_str(&w, "msg_uuid", msg_uuid, strlen(msg_uuid));
		}
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent SET '%s' with nil", key);
		node_add_pending_msg(node, msg_uuid, strlen(msg_uuid), handler, port->id);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

result_t proto_send_actor_new(node_t *node, actor_t *actor, char *to_rt_uuid, uint32_t to_rt_uuid_len, result_t (*handler)(node_t*, char*, void*))
{
	char buffer[2000], *w = NULL, msg_uuid[UUID_BUFFER_SIZE];

	if (!node_can_add_pending_msg(node))
		return CC_RESULT_PENDING;

	gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = mp_encode_map(w, 5);
	{
		w = encode_str(&w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = encode_str(&w, "from_rt_uuid", node->id, strlen(node->id));
		w = encode_str(&w, "cmd", "ACTOR_NEW", 9);
		w = encode_str(&w, "msg_uuid", msg_uuid, strlen(msg_uuid));
		w = actor_serialize(node, actor, &w, false);
	}

	if (transport_send(node->transport_client, buffer, w - buffer) == CC_RESULT_SUCCESS) {
		log_debug("Sent ACTOR_NEW to '%s'", actor->migrate_to);
		node_add_pending_msg(node, msg_uuid, strlen(msg_uuid), handler, actor->id);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

static result_t proto_parse_reply(node_t *node, char *root)
{
	result_t result = CC_RESULT_SUCCESS;
	char *msg_uuid = NULL, *r = root;
	pending_msg_t pending_msg;
	uint32_t len = 0;

	log_debug("proto_parse_reply");

	if (decode_string_from_map(r, "msg_uuid", &msg_uuid, &len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (node_get_pending_msg(node, msg_uuid, len, &pending_msg) != CC_RESULT_SUCCESS) {
		log_error("No pending messge with id '%.*s'", (int)len, msg_uuid);
		return CC_RESULT_FAIL;
	}

	result = pending_msg.handler(node, root, pending_msg.msg_data);
	node_remove_pending_msg(node, msg_uuid, len);
	return result;
}

static result_t proto_parse_token(node_t *node, char *root)
{
	char *obj_value = NULL, *obj_token = NULL, *obj_data = NULL;
	char *port_id = NULL, *r = root;
	uint32_t sequencenbr = 0, port_id_len = 0;
	size_t size = 0;
	port_t *port = NULL;
	bool ack = true;

	if (get_value_from_map(r, "value", &obj_value) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(obj_value, "peer_port_id", &port_id, &port_id_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_uint_from_map(obj_value, "sequencenbr", &sequencenbr) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (get_value_from_map(obj_value, "token", &obj_token) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (get_value_from_map(obj_token, "data", &obj_data) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	size = get_size_of_value(obj_data);

	port = port_get(node, port_id, port_id_len);
	if (port == NULL) {
		log_error("No port with id '%.*s'", (int)port_id_len, port_id);
		return CC_RESULT_FAIL;
	}

	ack = node_handle_token(port, obj_data, size, sequencenbr) == CC_RESULT_SUCCESS ? true : false;

	return proto_send_token_reply(node, port, sequencenbr, ack);
}

static result_t proto_parse_token_reply(node_t *node, char *root)
{
	char *value = NULL, *r = root, *port_id = NULL, *status = NULL;
	uint32_t sequencenbr = 0, port_id_len = 0, status_len = 0;

	log_debug("proto_parse_token_reply");

	if (get_value_from_map(r, "value", &value) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(value, "port_id", &port_id, &port_id_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(value, "value", &status, &status_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_uint_from_map(value, "sequencenbr", &sequencenbr) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (strncmp(status, "ACK", status_len) == 0) {
		node_handle_token_reply(node, port_id, port_id_len, PORT_REPLY_TYPE_ACK, sequencenbr);
		return CC_RESULT_SUCCESS;
	} else if (strncmp(status, "NACK", status_len) == 0) {
		node_handle_token_reply(node, port_id, port_id_len, PORT_REPLY_TYPE_NACK, sequencenbr);
		return CC_RESULT_SUCCESS;
	} else if (strncmp(status, "ABORT", status_len) == 0) {
		node_handle_token_reply(node, port_id, port_id_len, PORT_REPLY_TYPE_ABORT, sequencenbr);
		return CC_RESULT_SUCCESS;
	}
	log_error("Unknown status '%s'", status);

	return CC_RESULT_FAIL;
}

static result_t proto_parse_tunnel_data(node_t *node, char *root)
{
	char *msg_uuid = NULL, *value = NULL, *cmd = NULL, *r = root;
	pending_msg_t pending_msg;
	uint32_t msg_uuid_len = 0, cmd_len = 0;
	result_t result = CC_RESULT_FAIL;

	if (get_value_from_map(r, "value", &value) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (has_key(value, "msg_uuid")) {
		if (decode_string_from_map(value, "msg_uuid", &msg_uuid, &msg_uuid_len) != CC_RESULT_SUCCESS)
			return CC_RESULT_FAIL;

		if (node_get_pending_msg(node, msg_uuid, msg_uuid_len, &pending_msg) == CC_RESULT_SUCCESS) {
			result = pending_msg.handler(node, root, pending_msg.msg_data);
			node_remove_pending_msg(node, msg_uuid, msg_uuid_len);
			return result;
		}
	}

	if (has_key(value, "cmd")) {
		if (decode_string_from_map(value, "cmd", &cmd, &cmd_len) != CC_RESULT_SUCCESS)
			return CC_RESULT_FAIL;

		if (strncmp(cmd, "TOKEN_REPLY", 11) == 0)
			return proto_parse_token_reply(node, root);
		else if (strncmp(cmd, "TOKEN", 5) == 0)
			return proto_parse_token(node, root);
		log_error("Unhandled tunnel cmd '%.*s'", (int)cmd_len, cmd);
		return CC_RESULT_FAIL;
	}

	log_error("Unknown message");
	return CC_RESULT_FAIL;
}

static result_t proto_parse_actor_new(node_t *node, char *root)
{
	result_t result = CC_RESULT_SUCCESS;
	actor_t *actor = NULL;
	char *from_rt_uuid = NULL, *msg_uuid = NULL, *r = root;
	uint32_t from_rt_uuid_len = 0, msg_uuid_len = 0;

	log_debug("proto_parse_actor_new");

	if (decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(r, "msg_uuid", &msg_uuid, &msg_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	actor = actor_create(node, root);
	if (actor == NULL) {
		log_error("Failed to create actor");
		result = CC_RESULT_FAIL;
	}

	if (result == CC_RESULT_SUCCESS)
		result = proto_send_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 200);
	else
		result = proto_send_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 500);

	return result;
}

static result_t proto_parse_actor_migrate(node_t *node, char *root)
{
	result_t result = CC_RESULT_SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *actor_id = NULL, *msg_uuid = NULL;
	actor_t *actor = NULL;
	uint32_t actor_id_len = 0, from_rt_uuid_len = 0, msg_uuid_len = 0;

	result = decode_string_from_map(r, "actor_id", &actor_id, &actor_id_len);

	if (result == CC_RESULT_SUCCESS)
		result = decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len);

	if (result == CC_RESULT_SUCCESS)
		result = decode_string_from_map(r, "msg_uuid", &msg_uuid, &msg_uuid_len);

	if (result == CC_RESULT_SUCCESS) {
		actor = actor_get(node, actor_id, actor_id_len);
		if (actor != NULL) {
			// TODO: Get correct rt to migrate to (not who sent the request)
			result = actor_migrate(node, actor, from_rt_uuid, from_rt_uuid_len);
		} else {
			log_error("No actor with id '%s'", actor_id);
			result = CC_RESULT_FAIL;
		}
	}

	if (result == CC_RESULT_SUCCESS)
		result = proto_send_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 200);
	else
		result = proto_send_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 500);

	return result;
}

static result_t proto_parse_app_destroy(node_t *node, char *root)
{
	result_t result = CC_RESULT_SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *msg_uuid = NULL;
	char *obj_actor_uuids = NULL, *actor_id = NULL;
	uint32_t i = 0, size = 0, from_rt_uuid_len = 0, msg_uuid_len = 0, actor_id_len = 0;
	actor_t *actor = NULL;

	log_debug("proto_parse_app_destroy");

	if (decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(r, "msg_uuid", &msg_uuid, &msg_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (get_value_from_map(r, "actor_uuids", &obj_actor_uuids) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	size = get_size_of_array(obj_actor_uuids);
	for (i = 0; i < size && result == CC_RESULT_SUCCESS; i++) {
		result = decode_string_from_array(obj_actor_uuids, i, &actor_id, &actor_id_len);
		if (result == CC_RESULT_SUCCESS) {
			actor = actor_get(node, actor_id, actor_id_len);
			if (actor != NULL)
				actor_free(node, actor, true);
			else
				log_error("No actor with '%.*s'", (int)actor_id_len, actor_id);
		}
	}

	if (result == CC_RESULT_SUCCESS)
		result = proto_send_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 200);
	else
		result = proto_send_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 500);

	return result;
}

static result_t proto_parse_port_disconnect(node_t *node, char *root)
{
	result_t result = CC_RESULT_SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *msg_uuid = NULL, *peer_port_id = NULL;
	uint32_t from_rt_uuid_len = 0, msg_uuid_len = 0, peer_port_id_len = 0;

	log_debug("proto_parse_port_disconnect");

	if (decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(r, "msg_uuid", &msg_uuid, &msg_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(r, "peer_port_id", &peer_port_id, &peer_port_id_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	result = port_handle_disconnect(node, peer_port_id, peer_port_id_len);

	if (result == CC_RESULT_SUCCESS)
		result = proto_send_port_disconnect_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 200);
	else
		result = proto_send_port_disconnect_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 500);

	return result;
}

static result_t proto_parse_port_connect(node_t *node, char *root)
{
	result_t result = CC_RESULT_SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *msg_uuid = NULL;
	char *peer_port_id = NULL, *tunnel_id = NULL;
	uint32_t peer_port_id_len = 0, tunnel_id_len = 0, from_rt_uuid_len = 0, msg_uuid_len = 0;

	log_debug("proto_parse_port_connect");

	if (decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(r, "msg_uuid", &msg_uuid, &msg_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(r, "peer_port_id", &peer_port_id, &peer_port_id_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(r, "tunnel_id", &tunnel_id, &tunnel_id_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	result = port_handle_connect(node, peer_port_id, peer_port_id_len, tunnel_id, tunnel_id_len);

	if (result == CC_RESULT_SUCCESS)
		result = proto_send_port_connect_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 200, peer_port_id, peer_port_id_len);
	else
		result = proto_send_port_connect_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 404, peer_port_id, peer_port_id_len);

	return result;
}

static result_t proto_parse_tunnel_new(node_t *node, char *root)
{
	result_t result = CC_RESULT_SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *msg_uuid = NULL;
	char *type = NULL, *tunnel_id;
	uint32_t from_rt_uuid_len = 0, msg_uuid_len = 0, type_len = 0, tunnel_id_len = 0;
	link_t *link = NULL;

	log_debug("proto_parse_tunnel_new");

	if (decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(r, "msg_uuid", &msg_uuid, &msg_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(r, "type", &type, &type_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(r, "tunnel_id", &tunnel_id, &tunnel_id_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (strncmp(type, "token", strlen("token")) != 0) {
		log_error("Unhandled tunnel type '%.*s'", type_len, type);
		result = CC_RESULT_FAIL;
	} else {
		link = link_get(node, from_rt_uuid, from_rt_uuid_len);
		if (link == NULL) {
			link = link_create(node, from_rt_uuid, from_rt_uuid_len, false);
			if (link == NULL) {
				log_error("Failed to create link for tunnel request from '%.*s'", from_rt_uuid_len, from_rt_uuid);
				result = CC_RESULT_FAIL;
			}
		}
	}

	if (result == CC_RESULT_SUCCESS)
		result = tunnel_handle_tunnel_new_request(node, from_rt_uuid, from_rt_uuid_len, tunnel_id, tunnel_id_len);

	if (result == CC_RESULT_SUCCESS)
		result = proto_send_tunnel_new_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 200, tunnel_id, tunnel_id_len);
	else {
		if (link != NULL)
			link_remove_ref(node, link);
		result = proto_send_tunnel_new_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 500, tunnel_id, tunnel_id_len);
	}

	return result;
}

static result_t proto_handle_join_reply(node_t *node, char *buffer)
{
	char *id = NULL, *serializer = NULL;

	serializer = strstr(buffer, SERIALIZER);
	if (serializer == NULL) {
		log_error("Failed to parse JOIN_REPLY");
		return CC_RESULT_FAIL;
	}

	id = strstr(buffer, "\"id\": \"");
	if (id == NULL) {
		log_error("Failed to parse JOIN_REPLY");
		return CC_RESULT_FAIL;
	}
	id = id + 7;

	node->transport_client->state = TRANSPORT_ENABLED;
	strncpy(node->transport_client->peer_id, id, strstr(id, "\"") - id);

	return CC_RESULT_SUCCESS;
}

result_t proto_parse_message(node_t *node, char *data)
{
	char *cmd = NULL, *r = data, *msg_uuid = NULL, *from_rt_uuid = NULL;
	int i = 0;
	uint32_t cmd_len = 0, msg_uuid_len = 0, from_rt_uuid_len = 0;

	if (node->transport_client->state != TRANSPORT_ENABLED)
		return proto_handle_join_reply(node, data);

	if (decode_string_from_map(r, "cmd", &cmd, &cmd_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	log_debug("Received command '%.*s'", (int)cmd_len, cmd);

	for (i = 0; i < NBR_OF_COMMANDS; i++) {
		if (strncmp(cmd, command_handlers[i].command, cmd_len) == 0)
			return command_handlers[i].handler(node, r);
	}

	log_error("Unhandled command '%.*s'", (int)cmd_len, cmd);

	if (decode_string_from_map(r, "msg_uuid", &msg_uuid, &msg_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	return proto_send_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 500);
}
