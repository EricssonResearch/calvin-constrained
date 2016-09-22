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

#include "common.h"
#include "transport.h"
#include "tunnel.h"
#include "actor.h"

#define MAX_ACTORS 4
#define MAX_TUNNELS 10

typedef struct pending_msg_t {
    char *msg_uuid;
    result_t (*handler)(char *data, void *msg_data);
    void *msg_data;
    struct pending_msg_t *next;
} pending_msg_t;

typedef struct node_t {
    uint32_t vid;
    uint32_t pid;
    char *node_id;
    char *proxy_node_id;
    char *schema;
    char *name;
    char proxy_ip[40];
    int proxy_port;
    transport_client_t *transport;
    tunnel_t *storage_tunnel;
    pending_msg_t *pending_msgs;
    actor_t *actors[MAX_ACTORS];
    tunnel_t *tunnels[MAX_TUNNELS];
} node_t;

node_t *get_node();
result_t add_pending_msg(char *msg_uuid, result_t (*handler)(char *data, void *msg_data), void *msg_data);
result_t remove_pending_msg(char *msg_uuid);
tunnel_t *get_token_tunnel(const char *tunnel_id);
tunnel_t *get_token_tunnel_from_peerid(const char *peer_id);
result_t add_token_tunnel(tunnel_t *tunnel);
result_t route_request_handler(char *data, void *msg_data);
result_t remove_token_tunnel(const char *peer_id);
result_t token_tunnel_reply_handler(char *data, void *msg_data);
result_t request_tunnel(const char *peer_id, const char *type, void *handler);
void client_connected();
result_t handle_token(char *port_id, token_t *token, uint32_t sequencenbr);
void handle_token_reply(char *port_id, bool acked);
result_t handle_tunnel_connected(char *tunnel_id);
void handle_data(char *data, int len);
result_t create_node(uint32_t vid, uint32_t pid, char *name);
result_t start_node(const char *address);
void node_run();
result_t loop_once();
void stop_node(bool terminate);

#endif /* NODE_H */