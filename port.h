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
#ifndef PORT_H
#define PORT_H

#include <stdbool.h>
#include "common.h"
#include "fifo.h"
#include "tunnel.h"

struct actor_t;
struct node_t;

typedef enum {
	IN,
	OUT
} port_direction_t;

typedef enum {
	PORT_PENDING,
	PORT_DISCONNECTED,
	PORT_CONNECTED
} port_state_t;

typedef enum {
	PORT_REPLY_TYPE_ACK,
	PORT_REPLY_TYPE_NACK,
	PORT_REPLY_TYPE_ABORT
} port_reply_type_t;

typedef struct port_t {
	char *port_id;
	char *peer_id;
	char *peer_port_id;
	char *port_name;
	port_direction_t direction;
	tunnel_t *tunnel;
	port_state_t state;
	fifo_t *fifo;
	bool is_local;
	struct port_t *local_connection;
	struct port_t *next;
	struct actor_t *actor;
} port_t;

result_t create_port(struct node_t *node, struct actor_t *actor, port_t **port, port_t **head, char *obj_port, char *obj_prev_connections, port_direction_t direction);
void free_port(struct node_t *node, port_t *port, bool remove_from_storage);
result_t connect_ports(struct node_t *node, port_t *ports);
result_t disconnect_port(struct node_t *node, port_t *port);
port_t *get_inport(struct node_t *node, const char *port_id);
port_t *get_outport(struct node_t *node, const char *port_id);
result_t handle_port_disconnect(struct node_t *node, const char *port_id);
result_t handle_port_connect(struct node_t *node, const char *port_id, const char *tunnel_id);
result_t tunnel_connected(struct node_t *node, tunnel_t *tunnel);

#endif /* PORT_H */
