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
#include "cc_common.h"
#include "cc_transport.h"
#include "cc_tunnel.h"
#include "cc_actor.h"
#include "cc_port.h"
#include "cc_link.h"
#include "../south/platform/cc_platform.h"

#define MAX_URIS 5

typedef enum {
	NODE_DO_START,
	NODE_PENDING,
	NODE_STARTED,
	NODE_STOP
} node_state_t;

typedef struct pending_msg_t {
	char msg_uuid[UUID_BUFFER_SIZE];
	result_t (*handler)(struct node_t *node, char *data, void *msg_data);
	void *msg_data;
} pending_msg_t;

typedef struct node_attributes_t {
    list_t* indexed_public_owner;
    list_t* indexed_public_address;
    list_t* indexed_public_node_name;
} node_attributes_t;

typedef struct node_t {
	node_state_t state;
	char id[UUID_BUFFER_SIZE];
	char name[50];
	char *storage_dir;
	pending_msg_t pending_msgs[MAX_PENDING_MSGS];
	void *platform;
  node_attributes_t* attributes;
	link_t *proxy_link;
	list_t *links;
	tunnel_t *storage_tunnel;
	list_t *tunnels;
	list_t *actors;
	transport_client_t *transport_client;
	list_t *calvinsys;
	char *proxy_uris[MAX_URIS];
	bool (*fire_actors)(struct node_t *node);
} node_t;

result_t node_add_pending_msg(node_t *node, char *msg_uuid, uint32_t msg_uuid_len, result_t (*handler)(node_t *node, char *data, void *msg_data), void *msg_data);
result_t node_remove_pending_msg(node_t *node, char *msg_uuid, uint32_t msg_uuid_len);
result_t node_get_pending_msg(node_t *node, const char *msg_uuid, uint32_t msg_uuid_len, pending_msg_t *pending_msg);
bool node_can_add_pending_msg(const node_t *node);
result_t node_handle_token(port_t *port, const char *data, const size_t size, uint32_t sequencenbr);
void node_handle_token_reply(node_t *node, char *port_id, uint32_t port_id_len, port_reply_type_t reply_type, uint32_t sequencenbr);
result_t node_handle_message(node_t *node, char *buffer, size_t len);
result_t node_init(node_t* node, char *name, char *proxy_uris);
result_t node_run(node_t *node);
#ifdef USE_PERSISTENT_STORAGE
void node_set_state(node_t *node);
#endif
#endif /* NODE_H */
