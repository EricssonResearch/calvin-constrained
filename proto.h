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
#ifndef PROTO_H
#define PROTO_H

#include "tunnel.h"
#include "common.h"
#include "node.h"
#include "transport.h"
#include "port.h"
#include "token.h"
#include "actor.h"

result_t send_node_setup(char *msg_uuid, char *node_id, char *to_rt_uuid, uint32_t vid, uint32_t pid, char *name, transport_client_t *connection);
result_t send_tunnel_request(char *msg_uuid, tunnel_t *tunnel, char *node_id, const char *type);
result_t send_port_connect(char *msg_uuid, char *node_id, port_t *port);
result_t send_port_disconnect(char *msg_uuid, char *node_id, port_t *port);
result_t send_token(char *node_id, port_t *port, token_t *token);
result_t send_token_reply(char *node_id, port_t *port, uint32_t sequencenbr, char *status);
result_t send_set_actor(char *msg_uuid, tunnel_t *tunnel, char *node_id, actor_t *actor);
result_t send_set_port(char *msg_uuid, tunnel_t *tunnel, char *node_id, actor_t *actor, port_t *port);
result_t send_remove_node(char *msg_uuid, node_t *node);
result_t send_remove_actor(char *msg_uuid, tunnel_t *tunnel, char *node_id, actor_t *actor);
result_t send_remove_port(char *msg_uuid, tunnel_t *tunnel, char *node_id, port_t *port);
result_t parse_message(node_t *node, char *data, transport_client_t *connection);

#endif /* PROTO_H */