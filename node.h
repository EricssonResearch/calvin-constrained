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

#include <stdbool.h>
#include "common.h"
#include "transport.h"
#include "tunnel.h"
#include "actor.h"
#include "port.h"
#include "link.h"

typedef enum {
	NODE_DO_JOIN,
	NODE_DO_CONNECT_STORAGE_TUNNEL,
	NODE_DO_CONFIGURE,
	NODE_STARTED,
	NODE_PENDING
} node_state_t;

typedef struct pending_msg_t {
	char msg_uuid[UUID_BUFFER_SIZE];
	result_t (*handler)(char *data, void *msg_data);
	void *msg_data;
} pending_msg_t;

typedef struct node_t {
	node_state_t state;
	char id[UUID_BUFFER_SIZE];
	char *name;
	char *capabilities;
	pending_msg_t pending_msgs[MAX_PENDING_MSGS];
	link_t *proxy_link;
	list_t *links;
	tunnel_t *storage_tunnel;
	list_t *tunnels;
	list_t *actors;
} node_t;

node_t *node_get();
result_t node_add_pending_msg(char *msg_uuid, uint32_t msg_uuid_len, result_t (*handler)(char *data, void *msg_data), void *msg_data);
result_t node_remove_pending_msg(char *msg_uuid, uint32_t msg_uuid_len);
result_t node_get_pending_msg(const char *msg_uuid, uint32_t msg_uuid_len, pending_msg_t *pending_msg);
bool node_can_add_pending_msg(const node_t *node);
result_t node_handle_token(port_t *port, const char *data, const size_t size, uint32_t sequencenbr);
void node_handle_token_reply(char *port_id, uint32_t port_id_len, port_reply_type_t reply_type, uint32_t sequencenbr);
void node_handle_data(char *data, int len);
void node_transmit(void);
result_t node_create(char *name, char *capabilities);
result_t node_start(const char *ssdp_iface, const char *proxy_iface, const int proxy_port);
void node_run(void);
bool node_loop_once(void);
void node_stop(bool terminate);

#endif /* NODE_H */
