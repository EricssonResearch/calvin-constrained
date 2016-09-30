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

result_t send_node_setup(const node_t *node, result_t (*handler)(char*, void*));
result_t send_route_request(const node_t *node, char *dest_peer_id, result_t (*handler)(char*, void*));
result_t send_tunnel_request(const node_t *node, const char *to_rt_uuid, const char *tunnel_id, const char *type, result_t (*handler)(char*, void*));
result_t send_tunnel_destroy(const node_t *node, const char *to_rt_uuid, const char *tunnel_id, result_t (*handler)(char*, void*));
result_t send_port_connect(const node_t *node, port_t *port, result_t (*handler)(char*, void*));
result_t send_port_disconnect(const node_t *node, port_t *port, result_t (*handler)(char*, void*));
result_t send_token(const node_t *node, port_t *port, token_t *token);
result_t send_token_reply(const node_t *node, port_t *port, uint32_t sequencenbr, char *status);
result_t send_set_actor(const node_t *node, const actor_t *actor, result_t (*handler)(char*, void*));
result_t send_set_port(const node_t *node, actor_t *actor, port_t *port, result_t (*handler)(char*, void*));
result_t send_get_port(const node_t *node, char *port_id, result_t (*handler)(char*, void*), void *msg_data);
result_t send_remove_node(const node_t *node, result_t (*handler)(char*, void*));
result_t send_remove_actor(const node_t *node, actor_t *actor, result_t (*handler)(char*, void*));
result_t send_remove_port(const node_t *node, port_t *port, result_t (*handler)(char*, void*));
result_t parse_message(node_t *node, char *data);

#endif /* PROTO_H */
