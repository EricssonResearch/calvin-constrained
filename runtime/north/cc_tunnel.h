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
#ifndef CC_TUNNEL_H
#define CC_TUNNEL_H

#include <stdint.h>
#include "cc_link.h"

struct cc_node_t;

typedef enum {
	CC_TUNNEL_DISCONNECTED,
	CC_TUNNEL_ENABLED,
	CC_TUNNEL_PENDING
} cc_tunnel_state_t;

typedef enum {
	CC_TUNNEL_TYPE_STORAGE,
	CC_TUNNEL_TYPE_TOKEN,
	CC_TUNNEL_TYPE_PROXY
} cc_tunnel_type_t;

typedef struct cc_tunnel_t {
	char id[CC_UUID_BUFFER_SIZE];
	cc_link_t *link;
	cc_tunnel_state_t state;
	uint8_t ref_count;
	cc_tunnel_type_t type;
} cc_tunnel_t;

cc_tunnel_t *cc_tunnel_create(struct cc_node_t *node, cc_tunnel_type_t type, cc_tunnel_state_t state, char *peer_id, uint32_t peer_id_len, char *tunnel_id, uint32_t tunnel_id_len);
char *cc_tunnel_serialize(const cc_tunnel_t *tunnel, char *buffer);
cc_tunnel_t *cc_tunnel_deserialize(struct cc_node_t *node, char *buffer);
cc_tunnel_t *cc_tunnel_get_from_id(struct cc_node_t *node, const char *tunnel_id, uint32_t tunnel_id_len);
cc_tunnel_t *cc_tunnel_get_from_peerid_and_type(struct cc_node_t *node, const char *peer_id, uint32_t peer_id_len, cc_tunnel_type_t type);
void cc_tunnel_free(struct cc_node_t *node, cc_tunnel_t *tunnel, bool unref_link);
void cc_tunnel_add_ref(cc_tunnel_t *tunnel);
void cc_tunnel_remove_ref(struct cc_node_t *node, cc_tunnel_t *tunnel);
cc_result_t cc_tunnel_handle_tunnel_new_request(struct cc_node_t *node, char *peer_id, uint32_t peer_id_len, char *tunnel_id, uint32_t tunnel_id_len);

#endif /* CC_TUNNEL_H */
