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
	transport_client_t *connection;
	tunnel_state_t state;
	int ref_count; // TODO: Disconnect tunnel if ref_count == 0
} tunnel_t;

tunnel_t *create_tunnel(char *peer_id, transport_client_t *connection);
void free_tunnel(tunnel_t* tunnel);
tunnel_t *get_token_tunnel(char *peer_id);
result_t tunnel_connected(char *tunnel_id);

#endif /* TUNNEL_H */