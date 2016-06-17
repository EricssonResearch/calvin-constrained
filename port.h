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

typedef enum {
	IN,
	OUT
} port_direction_t;

typedef enum {
	PORT_DISCONNECTED,
	PORT_CONNECTED
} port_state_t;

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
} port_t;

result_t create_port(port_t **port, port_t **head, char *obj_port, char *obj_prev_connections, port_direction_t direction);
void free_port(port_t *port, bool remove_from_storage);
result_t add_ports(struct actor_t *actor, port_t *ports);
port_t *get_inport(char *port_id);
port_t *get_outport(char *port_id);
port_t *get_inport_from_peer(char *port_id);
result_t connect_port(port_t *port);
result_t disconnect_port(port_t *port);
result_t handle_port_disconnect(char *port_id);
result_t port_connected(char *port_peer_id);
result_t handle_port_connect(char *port_id, char *peer_port_id);

#endif /* PORT_H */