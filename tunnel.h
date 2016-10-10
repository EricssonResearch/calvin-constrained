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

#include "link.h"

struct node_t;

typedef enum {
	TUNNEL_PENDING,
	TUNNEL_WORKING,
	TUNNEL_TERMINATED
} tunnel_state_t;

typedef enum {
	TUNNEL_TYPE_STORAGE,
	TUNNEL_TYPE_TOKEN
} tunnel_type_t;

typedef struct tunnel_t {
	char *tunnel_id;
	link_t *link;
	tunnel_state_t state;
	int ref_count;
	tunnel_type_t type;
} tunnel_t;

tunnel_t *create_tunnel_from_id(link_t *link, tunnel_type_t type, tunnel_state_t state, const char *tunnel_id);
tunnel_t *create_tunnel(link_t *link, tunnel_type_t type, tunnel_state_t state);
result_t add_tunnel(struct node_t *node, tunnel_t *tunnel);
result_t remove_tunnel(struct node_t *node, const char *tunnel_id);
void free_tunnel(struct node_t *node, tunnel_t *tunnel);
void tunnel_add_ref(tunnel_t *tunnel);
void tunnel_remove_ref(struct node_t *node, tunnel_t *tunnel);
tunnel_t *get_tunnel(struct node_t *node, const char *tunnel_id);
tunnel_t *get_tunnel_from_peerid(struct node_t *node, const char *peer_id);
result_t request_token_tunnel(struct node_t *node, tunnel_t *tunnel);

#endif /* TUNNEL_H */
