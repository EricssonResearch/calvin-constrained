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
#ifndef TUNNEL_H
#define TUNNEL_H

#include "transport.h"

typedef enum {
    TUNNEL_DISCONNECTED,
    TUNNEL_CONNECTED
} tunnel_state_t;

typedef struct tunnel_t {
    char *tunnel_id;
    char *peer_id;
    tunnel_state_t state;
    uint32_t ref_count;
} tunnel_t;

tunnel_t *create_tunnel(const char *peer_id);
tunnel_t *create_tunnel_with_id(const char *peer_id, const char *tunnel_id);
void free_tunnel(tunnel_t *tunnel);
void tunnel_client_connected(tunnel_t *tunnel);
void tunnel_client_disconnected(tunnel_t *tunnel);

#endif /* TUNNEL_H */