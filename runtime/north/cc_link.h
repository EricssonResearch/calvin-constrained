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
#ifndef CC_LINK_H
#define CC_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include "cc_common.h"

struct cc_node_t;

typedef struct cc_link_t {
	char peer_id[CC_UUID_BUFFER_SIZE];
	uint8_t ref_count;
	bool is_proxy;
} cc_link_t;

cc_link_t *cc_link_create(struct cc_node_t *node, const char *peer_id, uint32_t peer_id_len, bool is_proxy);
char *cc_link_serialize(const cc_link_t *link, char *buffer);
cc_link_t *cc_link_deserialize(struct cc_node_t *node, char *buffer);
void cc_link_free(struct cc_node_t *node, cc_link_t *link);
void cc_link_add_ref(cc_link_t *link);
void cc_link_remove_ref(struct cc_node_t *node, cc_link_t *link);
cc_link_t *cc_link_get(struct cc_node_t *node, const char *peer_id, uint32_t peer_id_len);
void cc_link_transmit(struct cc_node_t *node, cc_link_t *link);

#endif /* C_CLINK_H */
