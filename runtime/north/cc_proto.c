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
#include <string.h>
#include "cc_proto.h"
#include "coder/cc_coder.h"
#include "cc_transport.h"
#include "cc_link.h"
#include "runtime/south/platform/cc_platform.h"
#include "runtime/north/cc_common.h"

#define STRING_TRUE			"true"
#define STRING_FALSE		"false"
#define STRING_IN				"in"
#define STRING_OUT			"out"
#define NBR_OF_COMMANDS	9

struct command_handler_t {
	char command[50];
	cc_result_t (*handler)(cc_node_t *node, char *data, size_t data_len);
};

static cc_result_t cc_proto_parse_reply(cc_node_t *node, char *data, size_t data_len);
static cc_result_t cc_proto_parse_tunnel_data(cc_node_t *node, char *data, size_t data_len);
static cc_result_t cc_proto_parse_actor_new(cc_node_t *node, char *data, size_t data_len);
static cc_result_t cc_proto_parse_app_destroy(cc_node_t *node, char *data, size_t data_len);
static cc_result_t cc_proto_parse_port_disconnect(cc_node_t *node, char *data, size_t data_len);
static cc_result_t cc_proto_parse_port_connect(cc_node_t *node, char *data, size_t data_len);
static cc_result_t cc_proto_parse_tunnel_new(cc_node_t *node, char *data, size_t data_len);
static cc_result_t cc_proto_parse_actor_migrate(cc_node_t *node, char *data, size_t data_len);

struct command_handler_t command_handlers[NBR_OF_COMMANDS] = {
	{"REPLY", cc_proto_parse_reply},
	{"TUNNEL_DATA", cc_proto_parse_tunnel_data},
	{"ACTOR_NEW", cc_proto_parse_actor_new},
	{"APP_DESTROY", cc_proto_parse_app_destroy},
	{"PORT_DISCONNECT", cc_proto_parse_port_disconnect},
	{"PORT_CONNECT", cc_proto_parse_port_connect},
	{"TUNNEL_NEW", cc_proto_parse_tunnel_new},
	{"ACTOR_MIGRATE", cc_proto_parse_actor_migrate}
};

cc_result_t cc_proto_send_join_request(const cc_node_t *node, cc_transport_client_t *transport_client, const char *serializer)
{
	char buffer[600], *w = NULL, msg_uuid[CC_UUID_BUFFER_SIZE];
	int size = 0;

	cc_gen_uuid(msg_uuid, "MSGID_");

	memset(buffer, 0, 600);

	w = buffer + transport_client->prefix_len;
	size = snprintf(w,
		600 - transport_client->prefix_len,
		"{\"cmd\": \"JOIN_REQUEST\", \"id\": \"%s\", \"sid\": \"%s\", \"serializers\": [\"%s\"]}",
		node->id,
		msg_uuid,
		serializer);

	return cc_transport_send(transport_client, buffer, size + transport_client->prefix_len);
}

cc_result_t cc_proto_send_node_setup(cc_node_t *node, cc_msg_handler_t handler)
{
	char buffer[1000], *w = NULL, msg_uuid[CC_UUID_BUFFER_SIZE];
	cc_list_t *capabilities = node->calvinsys->capabilities;
	uint32_t nbr_of_capabilities = 0;

	memset(buffer, 0, 1000);

	if (node->transport_client == NULL)
		return CC_FAIL;

	cc_gen_uuid(msg_uuid, "MSGID_");

	while (capabilities != NULL) {
		nbr_of_capabilities++;
		capabilities = capabilities->next;
	}

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_tunnel->link->peer_id, strnlen(node->proxy_tunnel->link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->proxy_tunnel->id, strnlen(node->proxy_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 5);
		{
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
			w = cc_coder_encode_kv_str(w, "cmd", "CONFIG", 6);
			if (node->attributes != NULL)
				w = cc_coder_encode_kv_str(w, "attributes", node->attributes, strnlen(node->attributes, CC_ATTRIBUTE_BUFFER_SIZE));
			else
				w = cc_coder_encode_kv_nil(w, "attributes");
			w = cc_coder_encode_kv_array(w, "capabilities", nbr_of_capabilities);
			{
				capabilities = node->calvinsys->capabilities;
				while (capabilities != NULL) {
					cc_log_debug("Setting capability %s", capabilities->id);
					w = cc_coder_encode_str(w, capabilities->id, strnlen(capabilities->id, CC_UUID_BUFFER_SIZE));
						capabilities = capabilities->next;
				}
			}
			w = cc_coder_encode_kv_str(w, "port_property_capability", "runtime.constrained.1", 21);
			w = cc_coder_encode_kv_bool(w, "redeploy", 0);
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, NULL) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_wake_signal(cc_node_t *node, cc_msg_handler_t handler)
{
	char buffer[500], *w = NULL, msg_uuid[CC_UUID_BUFFER_SIZE];

	memset(buffer, 0, 500);

	if (node->transport_client == NULL)
		return CC_FAIL;

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_tunnel->link->peer_id, strnlen(node->proxy_tunnel->link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->proxy_tunnel->id, strnlen(node->proxy_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 2);
		{
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
			w = cc_coder_encode_kv_str(w, "cmd", "WAKEUP", 6);
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, NULL) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_get_actor_module(cc_node_t *node, const char *actor_type, cc_msg_handler_t handler)
{
	char buffer[1000], *w = NULL, msg_uuid[CC_UUID_BUFFER_SIZE];

	memset(buffer, 0, 1000);

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_tunnel->link->peer_id, strnlen(node->proxy_tunnel->link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->proxy_tunnel->id, strnlen(node->proxy_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 4);
		{
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
			w = cc_coder_encode_kv_str(w, "cmd", "GET_ACTOR_MODULE", 16);
			w = cc_coder_encode_kv_str(w, "actor_type", actor_type, strlen(actor_type));
			w = cc_coder_encode_kv_str(w, "compiler", "mpy-cross", 9);
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, NULL) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_req_match(cc_node_t *node, cc_actor_t *actor, char *requirements, uint32_t requirements_len, cc_msg_handler_t handler)
{
	char buffer[1000], *w = NULL, msg_uuid[CC_UUID_BUFFER_SIZE];

	memset(buffer, 0, 1000);

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_tunnel->link->peer_id, strnlen(node->proxy_tunnel->link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->proxy_tunnel->id, strnlen(node->proxy_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 5);
		{
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
			w = cc_coder_encode_kv_str(w, "cmd", "REQ_MATCH", 9);
			w = cc_coder_encode_kv_str(w, "actor_id", actor->id, strlen(actor->id));
			w = cc_coder_encode_kv_value(w, "requirements", requirements, requirements_len);
			w = cc_coder_encode_kv_uint(w, "max_placements", 1);
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, NULL) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_sleep_request(cc_node_t *node, uint32_t time_to_sleep, cc_msg_handler_t handler)
{
	char buffer[1000], *w = NULL, msg_uuid[CC_UUID_BUFFER_SIZE];

	memset(buffer, 0, 1000);

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_tunnel->link->peer_id, strnlen(node->proxy_tunnel->link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->proxy_tunnel->id, strnlen(node->proxy_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 3);
		{
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
			w = cc_coder_encode_kv_str(w, "cmd", "WILL_SLEEP", 10);
			w = cc_coder_encode_kv_uint(w, "time", time_to_sleep);
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, NULL) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_tunnel_request(cc_node_t *node, cc_tunnel_t *tunnel, cc_msg_handler_t handler)
{
	char buffer[1000], type[20], *w = NULL, msg_uuid[CC_UUID_BUFFER_SIZE];

	memset(buffer, 0, 1000);
	memset(type, 0, 20);

	if (tunnel->type == CC_TUNNEL_TYPE_STORAGE)
		strncpy(type, "storage", 7);
	else if (tunnel->type == CC_TUNNEL_TYPE_PROXY)
		strncpy(type, "proxy", 5);
	else if (tunnel->type == CC_TUNNEL_TYPE_TOKEN)
		strncpy(type, "token", 5);
	else {
		cc_log_error("Uknown tunnel type '%d'", tunnel->type);
		return CC_FAIL;
	}

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 7);
	{
		w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", tunnel->link->peer_id, strnlen(tunnel->link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_NEW", 10);
		w = cc_coder_encode_kv_str(w, "tunnel_id", tunnel->id, strnlen(tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "type", type, strlen(type));
		w = cc_coder_encode_kv_map(w, "policy", 0);
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, tunnel) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_tunnel_destroy(cc_node_t *node, cc_tunnel_t *tunnel, cc_msg_handler_t handler)
{
	char buffer[1000], *w = NULL, msg_uuid[CC_UUID_BUFFER_SIZE];

	memset(buffer, 0, 1000);

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", tunnel->link->peer_id, strnlen(tunnel->link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DESTROY", 14);
		w = cc_coder_encode_kv_str(w, "tunnel_id", tunnel->id, strnlen(tunnel->id, CC_UUID_BUFFER_SIZE));
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, tunnel->id) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t proto_send_reply(const cc_node_t *node, char *msg_uuid, char *to_rt_uuid, uint32_t to_rt_uuid_len, uint32_t status)
{
	char buffer[1000], *w = NULL;

	memset(buffer, 0, 1000);

	if (node->transport_client == NULL)
		return CC_FAIL;

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "REPLY", 5);
		w = cc_coder_encode_kv_map(w, "value", 3);
		{
			w = cc_coder_encode_kv_uint(w, "status", status);
			w = cc_coder_encode_kv_nil(w, "data");
			w = cc_coder_encode_kv_array(w, "success_list", 7);
			w = cc_coder_encode_uint(w, 200);
			w = cc_coder_encode_uint(w, 201);
			w = cc_coder_encode_uint(w, 202);
			w = cc_coder_encode_uint(w, 203);
			w = cc_coder_encode_uint(w, 204);
			w = cc_coder_encode_uint(w, 205);
			w = cc_coder_encode_uint(w, 206);
		}
	}

	return cc_transport_send(node->transport_client, buffer, w - buffer);
}

cc_result_t proto_send_tunnel_new_reply(const cc_node_t *node, char *msg_uuid, uint32_t msg_uuid_len, char *to_rt_uuid, uint32_t to_rt_uuid_len, uint32_t status, char *tunnel_id, uint32_t tunnel_id_len)
{
	char buffer[1000], *w = NULL;

	memset(buffer, 0, 1000);

	if (node->transport_client == NULL)
		return CC_FAIL;

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, msg_uuid_len);
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "REPLY", 5);
		w = cc_coder_encode_kv_map(w, "value", 3);
		{
			w = cc_coder_encode_kv_uint(w, "status", status);
			w = cc_coder_encode_kv_array(w, "success_list", 7);
			w = cc_coder_encode_uint(w, 200);
			w = cc_coder_encode_uint(w, 201);
			w = cc_coder_encode_uint(w, 202);
			w = cc_coder_encode_uint(w, 203);
			w = cc_coder_encode_uint(w, 204);
			w = cc_coder_encode_uint(w, 205);
			w = cc_coder_encode_uint(w, 206);
			w = cc_coder_encode_kv_map(w, "data", 1);
			{
				w = cc_coder_encode_kv_str(w, "tunnel_id", tunnel_id, tunnel_id_len);
			}
		}
	}

	return cc_transport_send(node->transport_client, buffer, w - buffer);
}

cc_result_t proto_send_route_request_reply(const cc_node_t *node, char *msg_uuid, uint32_t msg_uuid_len, char *to_rt_uuid, uint32_t to_rt_uuid_len, uint32_t status, char *dest_peer_id, uint32_t dest_peer_id_len)
{
	char buffer[1000], *w = NULL;

	memset(buffer, 0, 1000);

	if (node->transport_client == NULL)
		return CC_FAIL;

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, msg_uuid_len);
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "REPLY", 5);
		w = cc_coder_encode_kv_map(w, "value", 3);
		{
			w = cc_coder_encode_kv_uint(w, "status", status);
			w = cc_coder_encode_kv_array(w, "success_list", 7);
			w = cc_coder_encode_uint(w, 200);
			w = cc_coder_encode_uint(w, 201);
			w = cc_coder_encode_uint(w, 202);
			w = cc_coder_encode_uint(w, 203);
			w = cc_coder_encode_uint(w, 204);
			w = cc_coder_encode_uint(w, 205);
			w = cc_coder_encode_uint(w, 206);
			w = cc_coder_encode_kv_map(w, "data", 1);
			{
				w = cc_coder_encode_kv_str(w, "peer_id", dest_peer_id, dest_peer_id_len);
			}
		}
	}

	return cc_transport_send(node->transport_client, buffer, w - buffer);
}

cc_result_t cc_proto_send_port_connect_reply(const cc_node_t *node, char *msg_uuid, uint32_t msg_uuid_len, char *to_rt_uuid, uint32_t to_rt_uuid_len, uint32_t status, char *port_id, uint32_t port_id_len)
{
	char buffer[1000], *w = NULL;

	memset(buffer, 0, 1000);

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, msg_uuid_len);
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "REPLY", 5);
		w = cc_coder_encode_kv_map(w, "value", 3);
		{
			w = cc_coder_encode_kv_uint(w, "status", status);
			w = cc_coder_encode_kv_array(w, "success_list", 7);
			w = cc_coder_encode_uint(w, 200);
			w = cc_coder_encode_uint(w, 201);
			w = cc_coder_encode_uint(w, 202);
			w = cc_coder_encode_uint(w, 203);
			w = cc_coder_encode_uint(w, 204);
			w = cc_coder_encode_uint(w, 205);
			w = cc_coder_encode_uint(w, 206);
			w = cc_coder_encode_kv_map(w, "data", 1);
			{
				w = cc_coder_encode_kv_str(w, "port_id", port_id, port_id_len);
			}
		}
	}

	return cc_transport_send(node->transport_client, buffer, w - buffer);
}

cc_result_t cc_proto_send_port_disconnect_reply(const cc_node_t *node, char *msg_uuid, uint32_t msg_uuid_len, char *to_rt_uuid, uint32_t to_rt_uuid_len, uint32_t status)
{
	char buffer[1000], *w = NULL;

	memset(buffer, 0, 1000);

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, msg_uuid_len);
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "REPLY", 5);
		w = cc_coder_encode_kv_map(w, "value", 3);
		{
			w = cc_coder_encode_kv_uint(w, "status", status);
			w = cc_coder_encode_kv_array(w, "success_list", 7);
			w = cc_coder_encode_uint(w, 200);
			w = cc_coder_encode_uint(w, 201);
			w = cc_coder_encode_uint(w, 202);
			w = cc_coder_encode_uint(w, 203);
			w = cc_coder_encode_uint(w, 204);
			w = cc_coder_encode_uint(w, 205);
			w = cc_coder_encode_uint(w, 206);
			w = cc_coder_encode_kv_map(w, "data", 1);
			{
				// TODO: If any, include remaining tokens
				w = cc_coder_encode_kv_map(w, "remaining_tokens", 0);
			}
		}
	}

	return cc_transport_send(node->transport_client, buffer, w - buffer);
}

cc_result_t cc_proto_send_token(const cc_node_t *node, cc_port_t *port, cc_token_t *token, uint32_t sequencenbr)
{
	char buffer[1000], *w = NULL;

	memset(buffer, 0, 1000);

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", port->peer_id, strnlen(port->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", port->tunnel->id, strnlen(port->tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 5);
		{
			w = cc_coder_encode_kv_str(w, "cmd", "TOKEN", 5);
			w = cc_coder_encode_kv_uint(w, "sequencenbr", sequencenbr);
			w = cc_coder_encode_kv_str(w, "port_id", port->id, strnlen(port->id, CC_UUID_BUFFER_SIZE));
			w = cc_coder_encode_kv_str(w, "peer_port_id", port->peer_port_id, strnlen(port->peer_port_id, CC_UUID_BUFFER_SIZE));
			w = cc_token_encode(w, token, true);
		}
	}

	return cc_transport_send(node->transport_client, buffer, w - buffer);
}

cc_result_t cc_proto_send_port_connect(cc_node_t *node, cc_port_t *port, cc_msg_handler_t handler)
{
	char buffer[1000], *w = NULL, msg_uuid[CC_UUID_BUFFER_SIZE];

	memset(buffer, 0, 1000);

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 11);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", port->peer_id, strnlen(port->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "tunnel_id", port->tunnel->id, strnlen(port->tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "PORT_CONNECT", 12);
		w = cc_coder_encode_kv_nil(w, "peer_port_name");
		w = cc_coder_encode_kv_nil(w, "peer_actor_id");
		w = cc_coder_encode_kv_str(w, "peer_port_id", port->peer_port_id, strnlen(port->peer_port_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "port_id", port->id, strnlen(port->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_nil(w, "peer_port_properties");
		w = cc_coder_encode_kv_map(w, "port_properties", 3);
		w = cc_coder_encode_kv_str(w, "direction", port->direction == CC_PORT_DIRECTION_IN ? STRING_IN : STRING_OUT, strlen(port->direction == CC_PORT_DIRECTION_IN ? STRING_IN : STRING_OUT));
		w = cc_coder_encode_kv_str(w, "routing", "default", 7);
		w = cc_coder_encode_kv_uint(w, "nbr_peers", 1);
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, port->id) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_port_disconnect(cc_node_t *node, cc_port_t *port, cc_msg_handler_t handler)
{
	char buffer[1000], *w = NULL, msg_uuid[CC_UUID_BUFFER_SIZE];

	memset(buffer, 0, 1000);

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 10);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", port->peer_id, strnlen(port->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "tunnel_id", port->tunnel->id, strnlen(port->tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "PORT_DISCONNECT", 15);
		w = cc_coder_encode_kv_nil(w, "peer_port_name");
		w = cc_coder_encode_kv_nil(w, "peer_actor_id");
		w = cc_coder_encode_kv_str(w, "peer_port_id", port->peer_port_id, strnlen(port->peer_port_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "port_id", port->id, strnlen(port->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_nil(w, "peer_port_dir");
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, port->id) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_set_actor(cc_node_t *node, const cc_actor_t*actor, cc_msg_handler_t handler)
{
	int key_len = 0;
	char buffer[1000], *w = NULL, key[50] = "", msg_uuid[CC_UUID_BUFFER_SIZE];
	cc_list_t *list = NULL;
	cc_port_t *port = NULL;
	uint32_t ninports = 0, noutports = 0, replication_len = 0;
	uint32_t replication_index = 0;
	char *obj_replication_id = NULL, *replication_str = NULL;
	cc_list_t *item = NULL;

	memset(buffer, 0, 1000);

	key_len = snprintf(key, 50, "actor-%s", actor->id);

	cc_gen_uuid(msg_uuid, "MSGID_");

	ninports = cc_list_count(actor->in_ports);
	noutports = cc_list_count(actor->out_ports);

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_link->peer_id, strnlen(node->proxy_link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->storage_tunnel->id, strnlen(node->storage_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 4);
		{
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
			w = cc_coder_encode_kv_str(w, "cmd", "SET", 3);
			w = cc_coder_encode_kv_str(w, "key", key, key_len);
			item = cc_list_get(actor->private_attributes, "_replication_id");
			if (item == NULL || cc_coder_type_of(item->data) != CC_CODER_MAP) {
				w = cc_coder_encode_kv_map(w, "value", 6);
			} else {
				w = cc_coder_encode_kv_map(w, "value", 9);
			}
			{
				w = cc_coder_encode_kv_bool(w, "is_shadow", false);
				w = cc_coder_encode_kv_str(w, "name", actor->name, strlen(actor->name));
				w = cc_coder_encode_kv_str(w, "node_id", node->id, strlen(node->id));
				w = cc_coder_encode_kv_str(w, "type", actor->type, strlen(actor->type));
				w = cc_coder_encode_kv_array(w, "inports", ninports);
				{
					list = actor->in_ports;
					while (list != NULL) {
						port = (cc_port_t *)list->data;
						w = cc_coder_encode_map(w, 2);
						w = cc_coder_encode_kv_str(w, "id", port->id, strlen(port->id));
						w = cc_coder_encode_kv_str(w, "name", port->name, strlen(port->name));
						list = list->next;
					}
				}
				w = cc_coder_encode_kv_array(w, "outports", noutports);
				{
					list = actor->out_ports;
					while (list != NULL) {
						port = (cc_port_t *)list->data;
						w = cc_coder_encode_map(w, 2);
						w = cc_coder_encode_kv_str(w, "id", port->id, strlen(port->id));
						w = cc_coder_encode_kv_str(w, "name", port->name, strlen(port->name));
						list = list->next;
					}
				}
				if (item != NULL && cc_coder_type_of(item->data) == CC_CODER_MAP) {
					obj_replication_id = item->data;
					if (cc_coder_decode_string_from_map(obj_replication_id, "id", &replication_str, &replication_len) == CC_SUCCESS) {
						w = cc_coder_encode_kv_str(w, "replication_id", replication_str, replication_len);
					} else {
						w = cc_coder_encode_kv_nil(w, "replication_id");
					}
					if (cc_coder_decode_string_from_map(obj_replication_id, "original_actor_id", &replication_str, &replication_len) == CC_SUCCESS) {
						w = cc_coder_encode_kv_str(w, "replication_master_id", replication_str, replication_len);
					} else {
						w = cc_coder_encode_kv_nil(w, "replication_master_id");
					}
					if (cc_coder_get_value_from_map(obj_replication_id, "index", &replication_str) == CC_SUCCESS &&
						cc_coder_decode_uint(replication_str, &replication_index) == CC_SUCCESS) {
						w = cc_coder_encode_kv_uint(w, "replication_index", replication_index);
					} else {
						w = cc_coder_encode_kv_uint(w, "replication_index", 0);
					}
				}
			}
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, actor->id) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_remove_actor(cc_node_t *node, cc_actor_t*actor, cc_msg_handler_t handler)
{
	char buffer[1000], *w = NULL, key[50], msg_uuid[CC_UUID_BUFFER_SIZE];
	int key_len = 0;

	memset(buffer, 0, 1000);

	key_len = snprintf(key, 50, "actor-%s", actor->id);

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_link->peer_id, strnlen(node->proxy_link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->storage_tunnel->id, strnlen(node->storage_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 3);
		{
			w = cc_coder_encode_kv_str(w, "cmd", "DELETE", 6);
			w = cc_coder_encode_kv_str(w, "key", key, key_len);
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, NULL) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_remove_replica(cc_node_t *node, cc_actor_t *actor, bool node_also, cc_msg_handler_t handler)
{
	char buffer[1000], *w = NULL, key[80], id[40], msg_uuid[CC_UUID_BUFFER_SIZE];
	int key_len = 0;
	char *replication_id = NULL;
	uint32_t replication_id_len = 0;
	cc_list_t *replication_item = NULL;

	memset(buffer, 0, 1000);
	replication_item = cc_list_get(actor->private_attributes, "_replication_id");
	if (replication_item == NULL || cc_coder_type_of(replication_item->data) != CC_CODER_MAP ||
		cc_coder_decode_string_from_map(replication_item->data, "id", &replication_id, &replication_id_len) != CC_SUCCESS) {
			return CC_FAIL;
	}

	strncpy(id, replication_id, replication_id_len);
	id[replication_id_len] = '\0';

	cc_log_error("BANAN: '%s'", actor->id);
	cc_log_error("BANAN: '%ld'", replication_id_len);
	cc_log_error("BANAN: '%.*s'", replication_id_len, replication_id);

	memset(key, 0, 80);
	key_len = snprintf(key, 17 + replication_id_len, "replicas/actors/%s", id);

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_link->peer_id, strnlen(node->proxy_link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->storage_tunnel->id, strnlen(node->storage_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 5);
		{
			w = cc_coder_encode_kv_str(w, "cmd", "REMOVE_INDEX", 12);
			w = cc_coder_encode_kv_str(w, "prefix", "index-", 6);
			w = cc_coder_encode_kv_array(w, "index", 1);
			{
				w = cc_coder_encode_str(w, key, key_len);
			}
			w = cc_coder_encode_kv_str(w, "value", actor->id, strnlen(actor->id, CC_UUID_BUFFER_SIZE));
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, NULL) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) != CC_SUCCESS) {
			cc_node_remove_pending_msg(node, msg_uuid);
			return CC_FAIL;
		}
	} else
		return CC_FAIL;

	if (!node_also) {
		return CC_SUCCESS;
	}
	memset(buffer, 0, 1000);
	memset(key, 0, 80);
	key_len = snprintf(key, 16 + replication_id_len, "replicas/nodes/%s", id);

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_link->peer_id, strnlen(node->proxy_link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->storage_tunnel->id, strnlen(node->storage_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 5);
		{
			w = cc_coder_encode_kv_str(w, "cmd", "REMOVE_INDEX", 12);
			w = cc_coder_encode_kv_str(w, "prefix", "index-", 6);
			w = cc_coder_encode_kv_array(w, "index", 1);
			{
				w = cc_coder_encode_str(w, key, key_len);
			}
			w = cc_coder_encode_kv_str(w, "value", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, NULL) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_set_port(cc_node_t *node, cc_port_t *port, cc_msg_handler_t handler)
{
	char buffer[2000], *w = NULL, key[50] = "", msg_uuid[CC_UUID_BUFFER_SIZE];
	int key_len = 0;

	memset(buffer, 0, 2000);

	cc_gen_uuid(msg_uuid, "MSGID_");

	key_len = snprintf(key, 50, "port-%s", port->id);

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_link->peer_id, strnlen(node->proxy_link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->storage_tunnel->id, strnlen(node->storage_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 4);
		{
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
			w = cc_coder_encode_kv_str(w, "cmd", "SET", 3);
			w = cc_coder_encode_kv_str(w, "key", key, key_len);
			w = cc_coder_encode_kv_map(w, "value", 6);
			{
				w = cc_coder_encode_kv_array(w, "peers", 1);
				{
					w = cc_coder_encode_array(w, 2);
					{
						if (strnlen(port->peer_id, CC_UUID_BUFFER_SIZE) > 0)
							w = cc_coder_encode_str(w, port->peer_id, strlen(port->peer_id));
						else
							w = cc_coder_encode_nil(w);
						w = cc_coder_encode_str(w, port->peer_port_id, strlen(port->peer_port_id));
					}
				}
				w = cc_coder_encode_kv_map(w, "properties", 3);
				{
					if (port->direction == CC_PORT_DIRECTION_IN)
						w = cc_coder_encode_kv_str(w, "direction", "in", 2);
					else
						w = cc_coder_encode_kv_str(w, "direction", "out", 3);
					w = cc_coder_encode_kv_str(w, "routing", "default", 7);
					w = cc_coder_encode_kv_uint(w, "nbr_peers", 1);
				}
				w = cc_coder_encode_kv_str(w, "name", port->name, strlen(port->name));
				w = cc_coder_encode_kv_str(w, "node_id", node->id, strlen(node->id));
				if (port->state == CC_PORT_ENABLED)
					w = cc_coder_encode_kv_str(w, "connected", "true", 4);
				else
					w = cc_coder_encode_kv_str(w, "connected", "false", 5);
				w = cc_coder_encode_kv_str(w, "actor_id", port->actor->id, strlen(port->actor->id));
			}
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, (void *)port->id) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_get_port(cc_node_t *node, char *port_id, cc_msg_handler_t handler, void *msg_data)
{
	char buffer[1000], msg_uuid[CC_UUID_BUFFER_SIZE], *w = NULL, key[50] = "";
	int key_len = 0;

	memset(buffer, 0, 1000);

	key_len = snprintf(key, 50, "port-%s", port_id);
	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_link->peer_id, strnlen(node->proxy_link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->storage_tunnel->id, strnlen(node->storage_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 3);
		{
			w = cc_coder_encode_kv_str(w, "cmd", "GET", 3);
			w = cc_coder_encode_kv_str(w, "key", key, strnlen(key, key_len));
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, msg_data) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_remove_port(cc_node_t *node, cc_port_t *port, cc_msg_handler_t handler)
{
	char buffer[1000], *w = NULL, key[50] = "", msg_uuid[CC_UUID_BUFFER_SIZE];
	int key_len = 0;

	memset(buffer, 0, 1000);

	key_len = snprintf(key, 50, "port-%s", port->id);
	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", node->proxy_link->peer_id, strnlen(node->proxy_link->peer_id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", node->storage_tunnel->id, strnlen(node->storage_tunnel->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_map(w, "value", 3);
		{
			w = cc_coder_encode_kv_str(w, "cmd", "DELETE", 6);
			w = cc_coder_encode_kv_str(w, "key", key, key_len);
			w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
		}
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, port->id) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

cc_result_t cc_proto_send_actor_new(cc_node_t *node, cc_actor_t *actor, char *to_rt_uuid, uint32_t to_rt_uuid_len, cc_msg_handler_t handler)
{
	char buffer[3000] = {0}, *w = NULL, msg_uuid[CC_UUID_BUFFER_SIZE];

	cc_gen_uuid(msg_uuid, "MSGID_");

	w = buffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", to_rt_uuid, to_rt_uuid_len);
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "ACTOR_NEW", 9);
		w = cc_coder_encode_kv_str(w, "msg_uuid", msg_uuid, strnlen(msg_uuid, CC_UUID_BUFFER_SIZE));
		w = cc_actor_serialize(node, actor, w, false);
	}

	if (cc_node_add_pending_msg(node, msg_uuid, handler, actor->id) == CC_SUCCESS) {
		if (cc_transport_send(node->transport_client, buffer, w - buffer) == CC_SUCCESS)
			return CC_SUCCESS;
		cc_node_remove_pending_msg(node, msg_uuid);
	}

	return CC_FAIL;
}

static cc_result_t cc_proto_parse_reply(cc_node_t *node, char *data, size_t data_len)
{
	cc_result_t result = CC_SUCCESS;
	char *tmp = NULL, *r = data, msg_uuid[CC_UUID_BUFFER_SIZE];
	cc_pending_msg_t *pending_msg = NULL;
	uint32_t len = 0;

	memset(msg_uuid, 0, CC_UUID_BUFFER_SIZE);

	if (cc_coder_decode_string_from_map(r, "msg_uuid", &tmp, &len) != CC_SUCCESS)
		return CC_FAIL;

	strncpy(msg_uuid, tmp, len);
	msg_uuid[len] = '\0';

	pending_msg = cc_node_get_pending_msg(node, msg_uuid);
	if (pending_msg == NULL) {
		cc_log_debug("No pending messge with id '%s'", msg_uuid);
		return CC_SUCCESS;
	}

	result = pending_msg->handler(node, data, data_len, pending_msg->msg_data);
	cc_node_remove_pending_msg(node, msg_uuid);
	return result;
}

static cc_result_t proto_parse_token(cc_node_t *node, char *root)
{
	char respbuffer[400], *w = NULL;
	char *obj_value = NULL, *obj_token = NULL, *obj_data = NULL;
	char *from_rt_uuid = NULL, *tunnel_id = NULL, *port_id = NULL;
	char *peer_port_id = NULL, *r = root;
	uint32_t sequencenbr = 0, port_id_len = 0, peer_port_id_len = 0;
	uint32_t from_rt_uuid_len = 0, tunnel_id_len = 0;
	size_t size = 0;
	cc_port_t *port = NULL;
	bool ack = false;

	if (cc_coder_decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'from_rt_uuid'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(r, "tunnel_id", &tunnel_id, &tunnel_id_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'tunnel_id'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(r, "value", &obj_value) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'value'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(obj_value, "peer_port_id", &port_id, &port_id_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'peer_port_id'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(obj_value, "port_id", &peer_port_id, &peer_port_id_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'port_id'");
		return CC_FAIL;
	}

	if (cc_coder_decode_uint_from_map(obj_value, "sequencenbr", &sequencenbr) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'sequencenbr'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(obj_value, "token", &obj_token) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'token'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(obj_token, "data", &obj_data) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'data'");
		return CC_FAIL;
	}

	port = cc_port_get(node, port_id, port_id_len);
	if (port != NULL) {
		size = cc_coder_get_size_of_value(obj_data);
		if (cc_node_handle_token(port, obj_data, size, sequencenbr) == CC_SUCCESS)
			ack = true;
	}

	memset(respbuffer, 0, 400);
	w = respbuffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", from_rt_uuid, from_rt_uuid_len);
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", tunnel_id, tunnel_id_len);
		w = cc_coder_encode_kv_map(w, "value", 5);
		{
			w = cc_coder_encode_kv_str(w, "cmd", "TOKEN_REPLY", 11);
			w = cc_coder_encode_kv_uint(w, "sequencenbr", sequencenbr);
			w = cc_coder_encode_kv_str(w, "peer_port_id", port_id, port_id_len);
			w = cc_coder_encode_kv_str(w, "port_id", peer_port_id, peer_port_id_len);
			w = cc_coder_encode_kv_str(w, "value", ack ? "ACK" : "NACK", ack ? 3 : 4);
		}
	}

	return cc_transport_send(node->transport_client, respbuffer, w - respbuffer);
}

static cc_result_t proto_parse_token_reply(cc_node_t *node, char *root)
{
	char *value = NULL, *r = root, *port_id = NULL, *status = NULL;
	uint32_t sequencenbr = 0, port_id_len = 0, status_len = 0;

	cc_log_debug("proto_parse_token_reply");

	if (cc_coder_get_value_from_map(r, "value", &value) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(value, "port_id", &port_id, &port_id_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(value, "value", &status, &status_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_uint_from_map(value, "sequencenbr", &sequencenbr) != CC_SUCCESS)
		return CC_FAIL;

	if (strncmp(status, "ACK", status_len) == 0) {
		cc_node_handle_token_reply(node, port_id, port_id_len, CC_PORT_REPLY_TYPE_ACK, sequencenbr);
		return CC_SUCCESS;
	} else if (strncmp(status, "NACK", status_len) == 0) {
		cc_node_handle_token_reply(node, port_id, port_id_len, CC_PORT_REPLY_TYPE_NACK, sequencenbr);
		return CC_SUCCESS;
	} else if (strncmp(status, "ABORT", status_len) == 0) {
		cc_node_handle_token_reply(node, port_id, port_id_len, CC_PORT_REPLY_TYPE_ABORT, sequencenbr);
		return CC_SUCCESS;
	}
	cc_log_error("Unknown status '%s'", status);

	return CC_FAIL;
}

static cc_result_t proto_parse_destroy(cc_node_t *node, char *root)
{
	char *r = root, respbuffer[400], *w = NULL;
	char *from_rt_uuid = NULL, *tunnel_id = NULL, *obj_value = NULL, *method = NULL;
	uint32_t method_len = 0, from_rt_uuid_len = 0, tunnel_id_len = 0;

	if (cc_coder_decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'from_rt_uuid'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(r, "tunnel_id", &tunnel_id, &tunnel_id_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'tunnel_id'");
		return CC_FAIL;
	}

	if (cc_coder_get_value_from_map(r, "value", &obj_value) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'value'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(obj_value, "method", &method, &method_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'method'");
		return CC_FAIL;
	}

	node->state = CC_NODE_STOP;
	if (strncmp(method, "/migrate", 8) == 0) {
		cc_log_debug("Proto: Received stop node with migrate");
		node->stop_method = CC_NODE_STOP_MIGRATE;
	} else {
		cc_log_debug("Proto: Received stop node");
		node->stop_method = CC_NODE_STOP_CLEAN;
	}

	memset(respbuffer, 0, 400);
	w = respbuffer + node->transport_client->prefix_len;
	w = cc_coder_encode_map(w, 5);
	{
		w = cc_coder_encode_kv_str(w, "to_rt_uuid", from_rt_uuid, from_rt_uuid_len);
		w = cc_coder_encode_kv_str(w, "from_rt_uuid", node->id, strnlen(node->id, CC_UUID_BUFFER_SIZE));
		w = cc_coder_encode_kv_str(w, "cmd", "TUNNEL_DATA", 11);
		w = cc_coder_encode_kv_str(w, "tunnel_id", tunnel_id, tunnel_id_len);
		w = cc_coder_encode_kv_map(w, "value", 2);
		{
			w = cc_coder_encode_kv_str(w, "cmd", "DESTROY_REPLY", 13);
			w = cc_coder_encode_kv_uint(w, "value", 200);
		}
	}

	return cc_transport_send(node->transport_client, respbuffer, w - respbuffer);
}

static cc_result_t cc_proto_parse_tunnel_data(cc_node_t *node, char *root, size_t len)
{
	char msg_uuid[CC_UUID_BUFFER_SIZE], *tmp = NULL, *value = NULL, *cmd = NULL, *r = root;
	cc_pending_msg_t *pending_msg = NULL;
	uint32_t msg_uuid_len = 0, cmd_len = 0;
	cc_result_t result = CC_FAIL;

	if (cc_coder_get_value_from_map(r, "value", &value) != CC_SUCCESS) {
		cc_log_error("Failed to get 'value'");
		return CC_FAIL;
	}

	if (cc_coder_has_key(value, "msg_uuid")) {
		if (cc_coder_decode_string_from_map(value, "msg_uuid", &tmp, &msg_uuid_len) != CC_SUCCESS) {
			cc_log_error("Failed to get 'msg_uuid'");
			return CC_FAIL;
		}

		strncpy(msg_uuid, tmp, msg_uuid_len);
		msg_uuid[msg_uuid_len] = '\0';

		pending_msg = cc_node_get_pending_msg(node, msg_uuid);
		if (pending_msg == NULL) {
			cc_log_error("No pending msg with id '%s'", msg_uuid);
			return CC_FAIL;
		}

		result = pending_msg->handler(node, root, len, pending_msg->msg_data);
		cc_node_remove_pending_msg(node, msg_uuid);
		return result;
	}

	if (cc_coder_has_key(value, "cmd")) {
		if (cc_coder_decode_string_from_map(value, "cmd", &cmd, &cmd_len) != CC_SUCCESS)
			return CC_FAIL;

		if (strncmp(cmd, "TOKEN_REPLY", 11) == 0)
			return proto_parse_token_reply(node, root);
		else if (strncmp(cmd, "TOKEN", 5) == 0)
			return proto_parse_token(node, root);
		else if (strncmp(cmd, "DESTROY", 7) == 0)
			return proto_parse_destroy(node, root);
		cc_log_error("Unhandled tunnel cmd");
		return CC_FAIL;
	}

	cc_log_error("Unknown message");
	return CC_FAIL;
}

static cc_result_t cc_proto_parse_actor_new(cc_node_t *node, char *root, size_t len)
{
	cc_result_t result = CC_SUCCESS;
	cc_actor_t *actor = NULL;
	char *from_rt_uuid = NULL, msg_uuid[CC_UUID_BUFFER_SIZE], *tmp = NULL, *r = root;
	uint32_t from_rt_uuid_len = 0, msg_uuid_len = 0;

	cc_log_debug("cc_proto_parse_actor_new");

	if (cc_coder_decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(r, "msg_uuid", &tmp, &msg_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	strncpy(msg_uuid, tmp, msg_uuid_len);
	msg_uuid[msg_uuid_len] = '\0';

	actor = cc_actor_create(node, root);
	if (actor == NULL) {
		cc_log_error("Failed to create actor");
		result = CC_FAIL;
	}

	if (result == CC_SUCCESS)
		result = proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 200);
	else
		result = proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 500);

	return result;
}

static cc_result_t cc_proto_send_req_match_with_actor_reqs(cc_node_t *node, cc_actor_t *actor, char *deploy_reqs, cc_list_t *requires)
{
	char buffer[1000]; // TODO: Set a reasonable size
	char *w = buffer;
	char *req = NULL;
	cc_list_t *tmp_item = NULL;
	uint32_t i = 0, nbr_of_reqs = cc_coder_get_size_of_array(deploy_reqs), size = 0;

	w = cc_coder_encode_array(w, nbr_of_reqs + 1);
	{
		for (i = 0; i < nbr_of_reqs; i++) {
			if (cc_coder_get_value_from_array(deploy_reqs, i, &req) != CC_SUCCESS) {
				cc_log_error("Failed to get value");
				return CC_FAIL;
			}
			size = cc_coder_get_size_of_value(req);
			memcpy(w, req, size);
			w = w + size;
		}

		w = cc_coder_encode_map(w, 3);
		w = cc_coder_encode_kv_str(w, "op", "actor_reqs_match", 16);
		w = cc_coder_encode_kv_str(w, "type", "+", 1);
		w = cc_coder_encode_kv_map(w, "kwargs", 1);
		{
			w = cc_coder_encode_kv_array(w, "requires", cc_list_count(requires));
			{
				while (requires != NULL) {
					w = cc_coder_encode_str(w, requires->id, strlen(requires->id));
					tmp_item = requires;
					requires = requires->next;
					cc_platform_mem_free(tmp_item->id);
					cc_platform_mem_free(tmp_item);
				}
			}
		}
	}

	return cc_proto_send_req_match(node, actor, buffer, w - buffer, cc_actor_req_match_reply_handler);
}

static cc_result_t cc_proto_parse_actor_migrate(cc_node_t *node, char *root, size_t len)
{
	cc_result_t result = CC_FAIL;
	char *r = root, *from_rt_uuid = NULL, *actor_id = NULL, msg_uuid[CC_UUID_BUFFER_SIZE], *tmp = NULL;
	char *requirements = NULL, *peer_node_id = NULL;
	cc_actor_t *actor = NULL;
	uint32_t actor_id_len = 0, from_rt_uuid_len = 0, msg_uuid_len = 0, peer_node_id_len = 0;
	uint32_t requirements_len = 0;
	cc_list_t *requires = NULL;

	if (cc_coder_decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_SUCCESS) {
		cc_log_error("Failed to get 'from_rt_uuid'");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(r, "msg_uuid", &tmp, &msg_uuid_len) != CC_SUCCESS) {
		cc_log_error("Failed to get 'msg_uuid'");
		return CC_FAIL;
	}

	strncpy(msg_uuid, tmp, msg_uuid_len);
	msg_uuid[msg_uuid_len] = '\0';

	if (cc_coder_decode_string_from_map(r, "actor_id", &actor_id, &actor_id_len) != CC_SUCCESS) {
		cc_log_error("Failed to get 'actor_id'");
		proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 400);
		return CC_FAIL;
	}

	actor = cc_actor_get(node, actor_id, actor_id_len);
	if (actor == NULL) {
		cc_log_error("Non existing actor");
		proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 404);
		return CC_FAIL;
	}

	if (cc_coder_has_key(r, "peer_node_id")) {
		if (cc_coder_decode_string_from_map(r, "peer_node_id", &peer_node_id, &peer_node_id_len) != CC_SUCCESS) {
			cc_log_error("Failed to get 'peer_node_id'");
			proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 500);
			return CC_FAIL;
		}

		if (cc_actor_migrate(node, actor, peer_node_id, peer_node_id_len) == CC_SUCCESS)
			return proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 200);
		return proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 500);
	}

	if (cc_coder_get_value_from_map(r, "requirements", &requirements) != CC_SUCCESS) {
		cc_log_error("Failed to get 'requirements'");
		proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 400);
		return CC_FAIL;
	}

	cc_log("proto: Sending req match for actor '%s'", actor->id);
	if (actor->get_requires != NULL) {
		if (actor->get_requires(actor, &requires) != CC_SUCCESS) {
			cc_log_error("Failed to get requires");
			proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 500);
			return CC_FAIL;
		}
	}

	if (requires == NULL) {
		requirements_len = cc_coder_get_size_of_value(requirements);
		result = cc_proto_send_req_match(node,
			actor,
			requirements,
			requirements_len,
			cc_actor_req_match_reply_handler);
	} else
		result = cc_proto_send_req_match_with_actor_reqs(node, actor, requirements, requires);

	if (result == CC_SUCCESS)
		result = proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 200);
	else
	 	result = proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 500);

	return result;
}

static cc_result_t cc_proto_parse_app_destroy(cc_node_t *node, char *root, size_t len)
{
	cc_result_t result = CC_SUCCESS;
	char *r = root, *from_rt_uuid = NULL, msg_uuid[CC_UUID_BUFFER_SIZE], *tmp = NULL;
	char *obj_actor_uuids = NULL, *actor_id = NULL;
	uint32_t i = 0, size = 0, from_rt_uuid_len = 0, msg_uuid_len = 0, actor_id_len = 0;
	cc_actor_t*actor = NULL;

	cc_log_debug("cc_proto_parse_app_destroy");

	if (cc_coder_decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(r, "msg_uuid", &tmp, &msg_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	strncpy(msg_uuid, tmp, msg_uuid_len);
	msg_uuid[msg_uuid_len] = '\0';

	if (cc_coder_get_value_from_map(r, "actor_uuids", &obj_actor_uuids) != CC_SUCCESS)
		return CC_FAIL;

	size = cc_coder_get_size_of_array(obj_actor_uuids);
	for (i = 0; i < size && result == CC_SUCCESS; i++) {
		result = cc_coder_decode_string_from_array(obj_actor_uuids, i, &actor_id, &actor_id_len);
		if (result == CC_SUCCESS) {
			actor = cc_actor_get(node, actor_id, actor_id_len);
			if (actor != NULL)
				cc_actor_free(node, actor, true);
			else
				cc_log_error("Failed to get actor");
		}
	}

	if (result == CC_SUCCESS)
		result = proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 200);
	else
		result = proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 500);

	return result;
}

static cc_result_t cc_proto_parse_port_disconnect(cc_node_t *node, char *root, size_t len)
{
	cc_result_t result = CC_SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *msg_uuid = NULL, *peer_port_id = NULL;
	uint32_t from_rt_uuid_len = 0, msg_uuid_len = 0, peer_port_id_len = 0;

	cc_log_debug("cc_proto_parse_port_disconnect");

	if (cc_coder_decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(r, "msg_uuid", &msg_uuid, &msg_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(r, "peer_port_id", &peer_port_id, &peer_port_id_len) != CC_SUCCESS)
		return CC_FAIL;

	result = cc_port_handle_disconnect(node, peer_port_id, peer_port_id_len);

	if (result == CC_SUCCESS)
		result = cc_proto_send_port_disconnect_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 200);
	else
		result = cc_proto_send_port_disconnect_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 500);

	return result;
}

static cc_result_t cc_proto_parse_port_connect(cc_node_t *node, char *root, size_t len)
{
	cc_result_t result = CC_SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *msg_uuid = NULL;
	char *peer_port_id = NULL, *tunnel_id = NULL;
	uint32_t peer_port_id_len = 0, tunnel_id_len = 0, from_rt_uuid_len = 0, msg_uuid_len = 0;

	cc_log_debug("cc_proto_parse_port_connect");

	if (cc_coder_decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(r, "msg_uuid", &msg_uuid, &msg_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(r, "peer_port_id", &peer_port_id, &peer_port_id_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(r, "tunnel_id", &tunnel_id, &tunnel_id_len) != CC_SUCCESS)
		return CC_FAIL;

	result = cc_port_handle_connect(node, peer_port_id, peer_port_id_len, tunnel_id, tunnel_id_len);

	if (result == CC_SUCCESS)
		result = cc_proto_send_port_connect_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 200, peer_port_id, peer_port_id_len);
	else
		result = cc_proto_send_port_connect_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 404, peer_port_id, peer_port_id_len);

	return result;
}

static cc_result_t cc_proto_parse_tunnel_new(cc_node_t *node, char *root, size_t len)
{
	cc_result_t result = CC_SUCCESS;
	char *r = root, *from_rt_uuid = NULL, *msg_uuid = NULL;
	char *type = NULL, *tunnel_id;
	uint32_t from_rt_uuid_len = 0, msg_uuid_len = 0, type_len = 0, tunnel_id_len = 0;
	cc_link_t *link = NULL;

	cc_log_debug("cc_proto_parse_tunnel_new");

	if (cc_coder_decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(r, "msg_uuid", &msg_uuid, &msg_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(r, "type", &type, &type_len) != CC_SUCCESS)
		return CC_FAIL;

	if (cc_coder_decode_string_from_map(r, "tunnel_id", &tunnel_id, &tunnel_id_len) != CC_SUCCESS)
		return CC_FAIL;

	if (strncmp(type, "token", 5) != 0) {
		cc_log_error("Unhandled tunnel type");
		result = CC_FAIL;
	} else {
		link = cc_link_get(node, from_rt_uuid, from_rt_uuid_len);
		if (link == NULL) {
			link = cc_link_create(node, from_rt_uuid, from_rt_uuid_len, false);
			if (link == NULL) {
				cc_log_error("Failed to create link");
				result = CC_FAIL;
			}
		}
	}

	if (result == CC_SUCCESS)
		result = cc_tunnel_handle_tunnel_new_request(node, from_rt_uuid, from_rt_uuid_len, tunnel_id, tunnel_id_len);

	if (result == CC_SUCCESS)
		result = proto_send_tunnel_new_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 200, tunnel_id, tunnel_id_len);
	else {
		if (link != NULL)
			cc_link_remove_ref(node, link);
		result = proto_send_tunnel_new_reply(node, msg_uuid, msg_uuid_len, from_rt_uuid, from_rt_uuid_len, 500, tunnel_id, tunnel_id_len);
	}

	return result;
}

static cc_result_t proto_handle_join_reply(cc_node_t *node, char *buffer, uint32_t buffer_len)
{
	char *serializer = NULL;
	jsmn_parser parser;
	jsmntok_t tokens[10], *token = NULL;
	int res = 0;

	jsmn_init(&parser);
	res = jsmn_parse(&parser, buffer, buffer_len, tokens, sizeof(tokens) / sizeof(tokens[0]));

	if (res < 0) {
		cc_log_error("Failed to parse JSON: %d", res);
		return CC_FAIL;
	}

	token = cc_json_get_dict_value(buffer, &tokens[0], parser.toknext, "serializer", 10);
	if (token == NULL) {
		cc_log_error("Failed to get 'serializer'");
		return CC_FAIL;
	}

	serializer = cc_coder_get_name();
	if (serializer == NULL) {
		cc_log_error("Failed to get serializer");
		return CC_FAIL;
	}

	if (strlen(serializer) != token->end - token->start ||
			strncmp(serializer, buffer + token->start, strlen(serializer)) != 0) {
		cc_log_error("Unsupported serializer");
		cc_platform_mem_free((void *)serializer);
		return CC_FAIL;
	}

	cc_platform_mem_free((void *)serializer);

	token = cc_json_get_dict_value(buffer, &tokens[0], parser.toknext, "id", 2);
	if (token == NULL) {
		cc_log_error("Failed to get 'id'");
		return CC_FAIL;
	}

	memset(node->transport_client->peer_id, 0, CC_UUID_BUFFER_SIZE);
	strncpy(node->transport_client->peer_id, buffer + token->start, token->end - token->start);
	node->transport_client->state = CC_TRANSPORT_ENABLED;

	return CC_SUCCESS;
}

cc_result_t cc_proto_parse_message(cc_node_t *node, char *data, size_t data_len)
{
	char *cmd = NULL, *r = data, msg_uuid[CC_UUID_BUFFER_SIZE], *tmp = NULL, *from_rt_uuid = NULL;
	int i = 0;
	uint32_t cmd_len = 0, msg_uuid_len = 0, from_rt_uuid_len = 0;

	if (node->transport_client->state != CC_TRANSPORT_ENABLED)
		return proto_handle_join_reply(node, data, data_len);

	if (cc_coder_decode_string_from_map(r, "cmd", &cmd, &cmd_len) != CC_SUCCESS)
		return CC_FAIL;

	for (i = 0; i < NBR_OF_COMMANDS; i++) {
		if (strncmp(cmd, command_handlers[i].command, cmd_len) == 0)
			return command_handlers[i].handler(node, data, data_len);
	}

	cc_log_error("Unhandled command");

	if (cc_coder_decode_string_from_map(r, "msg_uuid", &tmp, &msg_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	strncpy(msg_uuid, tmp, msg_uuid_len);
	msg_uuid[msg_uuid_len] = '\0';

	if (cc_coder_decode_string_from_map(r, "from_rt_uuid", &from_rt_uuid, &from_rt_uuid_len) != CC_SUCCESS)
		return CC_FAIL;

	return proto_send_reply(node, msg_uuid, from_rt_uuid, from_rt_uuid_len, 500);
}
