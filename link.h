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
#ifndef LINK_H
#define LINK_H

#include "common.h"

struct node_t;

typedef enum {
	LINK_PENDING,
	LINK_WORKING
} link_state_t;

typedef struct link_t {
	char *peer_id;
	link_state_t state;
} link_t;

link_t *create_link(char *peer_id, link_state_t state);
result_t add_link(struct node_t *node, link_t *link);
link_t *get_link(struct node_t *node, char *peer_id);
result_t request_link(struct node_t *node, link_t *link);
void remove_link(struct node_t *node, link_t *link);
void free_link(link_t *link);

#endif /* LINK_H */
