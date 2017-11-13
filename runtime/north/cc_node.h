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
#ifndef CC_NODE_H
#define CC_NODE_H

#include <stdbool.h>
#include "cc_common.h"
#include "cc_transport.h"
#include "cc_tunnel.h"
#include "cc_actor.h"
#include "cc_port.h"
#include "cc_link.h"
#include "calvinsys/cc_calvinsys.h"
#include "runtime/south/platform/cc_platform.h"

#define CC_INDEFINITELY_TIMEOUT 0

typedef enum {
	CC_NODE_DO_START,
	CC_NODE_DO_SLEEP,
	CC_NODE_PENDING,
	CC_NODE_STARTED,
	CC_NODE_STOP
} cc_node_state_t;

typedef struct cc_pending_msg_t {
	cc_result_t (*handler)(struct cc_node_t *node, char *data, size_t data_len, void *msg_data);
	void *msg_data;
} cc_pending_msg_t;

typedef struct cc_node_t {
	cc_node_state_t state;
	char id[CC_UUID_BUFFER_SIZE];
	char *storage_dir;
	char *attributes;
	cc_list_t *pending_msgs;
	void *platform;
	cc_link_t *proxy_link;
	cc_list_t *links;
	cc_tunnel_t *storage_tunnel;
	cc_tunnel_t *proxy_tunnel;
	cc_list_t *tunnels;
	cc_list_t *actor_types;
	cc_list_t *actors;
	cc_transport_client_t *transport_client;
	cc_calvinsys_t *calvinsys;
	cc_list_t *proxy_uris;
	uint32_t seconds_since_epoch;
	uint32_t time_at_sync;
	bool (*fire_actors)(struct cc_node_t *node);
} cc_node_t;

cc_result_t cc_node_add_pending_msg(cc_node_t *node, char *msg_uuid, cc_result_t (*handler)(cc_node_t *node, char *data, size_t data_len, void *msg_data), void *msg_data);
void cc_node_remove_pending_msg(cc_node_t *node, char *msg_uuid);
cc_pending_msg_t *cc_node_get_pending_msg(cc_node_t *node, const char *msg_uuid);
bool cc_node_can_add_pending_msg(const cc_node_t *node);
cc_result_t cc_node_handle_token(cc_port_t *port, const char *data, const size_t size, uint32_t sequencenbr);
void cc_node_handle_token_reply(cc_node_t *node, char *port_id, uint32_t port_id_len, cc_port_reply_type_t reply_type, uint32_t sequencenbr);
cc_result_t cc_node_handle_message(cc_node_t *node, char *buffer, size_t len);
cc_result_t cc_node_init(cc_node_t *node, const char *attributes, const char *proxy_uris, const char *storage_dir);
uint32_t cc_node_get_time(cc_node_t *node);
cc_result_t cc_node_run(cc_node_t *node);
#ifdef CC_STORAGE_ENABLED
void cc_node_set_state(cc_node_t *node);
#endif
#endif /* CC_NODE_H */
