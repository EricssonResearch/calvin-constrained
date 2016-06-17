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
#ifndef NODE_H
#define NODE_H

#include "common.h"
#include "transport.h"
#include "tunnel.h"
#include "actor.h"

#define MAX_ACTORS 4
#define MAX_TUNNELS 10

typedef enum {
	PROXY_CONFIG,
	JOIN_REPLY,
	TUNNEL_NEW,
	REPLY,
	STORAGE_REPLY,
	ACTOR_NEW_REQUEST,
	TOKEN,
	TOKEN_REPLY,
	STORAGE_SET,
	PORT_CONNECT,
	PORT_DISCONNECT,
	ACTOR_NEW
} msg_type_t;

typedef struct pending_msg_t {
	char *msg_uuid;
	msg_type_t type;
	void *data;
	struct pending_msg_t *next;
} pending_msg_t;

typedef struct node_t {
	uint32_t vid;
	uint32_t pid;
	char *node_id;
	char *schema;
	char *name;
	char *proxy_ip;
	int proxy_port;
	transport_client_t *client;
	tunnel_t *storage_tunnel;
	pending_msg_t *pending_msgs;
	actor_t *actors[MAX_ACTORS];
	tunnel_t *tunnels[MAX_TUNNELS];
} node_t;

node_t *get_node();
result_t add_pending_msg_with_data(msg_type_t type, char *msg_uuid, void *data);
result_t add_pending_msg(msg_type_t type, char *msg_uuid);
result_t remove_pending_msg(char *msg_uuid);
void client_connected();
result_t handle_token(char *port_id, token_t *token, uint32_t sequencenbr);
void handle_token_reply(char *port_id, bool acked);
result_t handle_tunnel_connected(char *tunnel_id);
void handle_data(char *data, int len, transport_client_t *connection);
result_t create_node(uint32_t vid, uint32_t pid, char *name, char *address, int port);
result_t start_node();
void node_run();
result_t loop_once();
void stop_node();

#endif /* NODE_H */