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
#ifndef CC_PORT_H
#define CC_PORT_H

#include <stdbool.h>
#include "cc_common.h"
#include "cc_fifo.h"
#include "cc_tunnel.h"

#define CC_MAX_PORT_NAME_LENGTH		20

struct cc_actor_t;
struct cc_node_t;

typedef enum {
	CC_PORT_DIRECTION_IN,
	CC_PORT_DIRECTION_OUT
} cc_port_direction_t;

typedef enum {
	CC_PORT_DISCONNECTED,
	CC_PORT_ENABLED,
	CC_PORT_PENDING,
} cc_port_state_t;

typedef enum {
	CC_PORT_REPLY_TYPE_ACK,
	CC_PORT_REPLY_TYPE_NACK,
	CC_PORT_REPLY_TYPE_ABORT
} cc_port_reply_type_t;

typedef struct cc_port_t {
	char id[CC_UUID_BUFFER_SIZE];
	char name[CC_MAX_PORT_NAME_LENGTH];
	char peer_port_id[CC_UUID_BUFFER_SIZE];
	cc_port_direction_t direction;
	struct cc_port_t *peer_port;
	cc_tunnel_t *tunnel;
	cc_port_state_t state;
	cc_fifo_t *fifo;
	struct cc_actor_t*actor;
} cc_port_t;

cc_port_t *cc_port_create(struct cc_node_t *node, struct cc_actor_t*actor, char *obj_port, char *obj_prev_connections, cc_port_direction_t direction);
void cc_port_free(struct cc_node_t *node, cc_port_t *port, bool remove_from_registry);
char *cc_port_get_peer_id(const struct cc_node_t *node, cc_port_t *port);
cc_port_t *cc_port_get(struct cc_node_t *node, const char *port_id, uint32_t port_id_len);
cc_port_t *cc_port_get_from_name(struct cc_actor_t*actor, const char *name, size_t name_len, cc_port_direction_t direction);
cc_result_t cc_port_handle_disconnect(struct cc_node_t *node, const char *port_id, uint32_t port_id_len);
cc_result_t cc_port_handle_connect(struct cc_node_t *node, const char *port_id, uint32_t port_id_len, const char *tunnel_id, uint32_t tunnel_id_len);
void cc_port_disconnect(struct cc_node_t *node, cc_port_t *port);
void cc_port_transmit(struct cc_node_t *node, cc_port_t *port);

#endif /* CC_PORT_H */
