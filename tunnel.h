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

#include <stdint.h>
#include "link.h"

struct node_t;

typedef enum {
	TUNNEL_DO_CONNECT,
	TUNNEL_DO_DISCONNECT,
	TUNNEL_ENABLED,
	TUNNEL_PENDING,
	TUNNEL_CONNECT_FAILED
} tunnel_state_t;

typedef enum {
	TUNNEL_TYPE_NONE,
	TUNNEL_TYPE_STORAGE,
	TUNNEL_TYPE_TOKEN
} tunnel_type_t;

typedef struct tunnel_t {
	char tunnel_id[UUID_BUFFER_SIZE];
	link_t *link;
	tunnel_state_t state;
	int ref_count;
	tunnel_type_t type;
} tunnel_t;

tunnel_t *tunnel_create(struct node_t *node, tunnel_type_t type, tunnel_state_t state, char *peer_id, uint32_t peer_id_len, char *tunnel_id, uint32_t tunnel_id_len);
void tunnel_free(struct node_t *node, tunnel_t *tunnel);
tunnel_t *tunnel_get_from_id(struct node_t *node, const char *tunnel_id, uint32_t tunnel_id_len, tunnel_type_t type);
tunnel_t *tunnel_get_from_peerid(struct node_t *node, const char *peer_id, uint32_t peer_id_len, tunnel_type_t type);
void tunnel_add_ref(tunnel_t *tunnel);
void tunnel_remove_ref(struct node_t *node, tunnel_t *tunnel);
result_t tunnel_handle_tunnel_new_request(struct node_t *node, char *peer_id, uint32_t peer_id_len, char *tunnel_id, uint32_t tunnel_id_len);
result_t tunnel_transmit(struct node_t *node, tunnel_t *tunnel);

#endif /* TUNNEL_H */
