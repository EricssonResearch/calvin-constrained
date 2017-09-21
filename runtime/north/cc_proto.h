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
#ifndef CC_PROTO_H
#define CC_PROTO_H

#include "cc_tunnel.h"
#include "cc_common.h"
#include "cc_node.h"
#include "cc_transport.h"
#include "cc_port.h"
#include "cc_token.h"
#include "cc_actor.h"
#include "cc_tunnel.h"

cc_result_t cc_proto_send_join_request(const cc_node_t *node, cc_transport_client_t *transport_client, const char *serializer);
cc_result_t cc_proto_send_node_setup(cc_node_t *node, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*));
cc_result_t cc_proto_send_sleep_request(cc_node_t *node, uint32_t seconds_to_sleep, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*));
cc_result_t cc_proto_send_tunnel_request(cc_node_t *node, cc_tunnel_t *tunnel, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*));
cc_result_t cc_proto_send_tunnel_destroy(cc_node_t *node, cc_tunnel_t *tunnel, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*));
cc_result_t cc_proto_send_port_connect(cc_node_t *node, cc_port_t *port, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*));
cc_result_t cc_proto_send_cc_port_disconnect(cc_node_t *node, cc_port_t *port, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*));
cc_result_t cc_proto_send_token(const cc_node_t *node, cc_port_t *port, cc_token_t *token, uint32_t sequencenbr);
cc_result_t cc_proto_send_token_reply(const cc_node_t *node, cc_port_t *port, uint32_t sequencenbr, bool ack);
cc_result_t cc_proto_send_set_actor(cc_node_t *node, const cc_actor_t*actor, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*));
cc_result_t cc_proto_send_set_port(cc_node_t *node, cc_port_t *port, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*));
cc_result_t cc_proto_send_get_port(cc_node_t *node, char *port_id, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*), void *msg_data);
cc_result_t cc_proto_send_remove_actor(cc_node_t *node, cc_actor_t*actor, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*));
cc_result_t cc_proto_send_remove_port(cc_node_t *node, cc_port_t *port, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*));
cc_result_t cc_proto_send_actor_new(cc_node_t *node, cc_actor_t*actor, char *to_rt_uuid, uint32_t to_rt_uuid_len, cc_result_t (*handler)(cc_node_t*, char*, size_t, void*));
cc_result_t cc_proto_parse_message(cc_node_t *node, char *data, size_t data_len);

#endif /* CC_PROTO_H */
