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
#include "tunnel.h"

result_t proto_send_join_request(const node_t *node, transport_client_t *transport_client, const char *serializer);
result_t proto_send_node_setup(node_t *node, result_t (*handler)(node_t*, char*, void*));
result_t proto_send_tunnel_request(node_t *node, tunnel_t *tunnel, result_t (*handler)(node_t*, char*, void*));
result_t proto_send_tunnel_destroy(node_t *node, tunnel_t *tunnel, result_t (*handler)(node_t*, char*, void*));
result_t proto_send_port_connect(node_t *node, port_t *port, result_t (*handler)(node_t*, char*, void*));
result_t proto_send_port_disconnect(node_t *node, port_t *port, result_t (*handler)(node_t*, char*, void*));
result_t proto_send_token(const node_t *node, port_t *port, token_t token, uint32_t sequencenbr);
result_t proto_send_token_reply(const node_t *node, port_t *port, uint32_t sequencenbr, bool ack);
result_t proto_send_set_actor(node_t *node, const actor_t *actor, result_t (*handler)(node_t*, char*, void*));
result_t proto_send_set_port(node_t *node, port_t *port, result_t (*handler)(node_t*, char*, void*));
result_t proto_send_get_port(node_t *node, char *port_id, result_t (*handler)(node_t*, char*, void*), void *msg_data);
result_t proto_send_remove_actor(node_t *node, actor_t *actor, result_t (*handler)(node_t*, char*, void*));
result_t proto_send_remove_port(node_t *node, port_t *port, result_t (*handler)(node_t*, char*, void*));
result_t proto_send_actor_new(node_t *node, actor_t *actor, char *to_rt_uuid, uint32_t to_rt_uuid_len, result_t (*handler)(node_t*, char*, void*));
result_t proto_parse_message(node_t *node, char *data);

#endif /* PROTO_H */
