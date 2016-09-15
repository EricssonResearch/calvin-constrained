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
#ifndef ACTORS_H
#define ACTORS_H

#include <stdbool.h>
#include "common.h"
#include "port.h"

struct node_t;

typedef struct managed_attributes_t {
	char *attribute;
	struct managed_attributes_t *next;
} managed_attributes_t;

typedef struct actor_state_t {
	int nbr_attributes;
	void *state;
} actor_state_t;

typedef struct actor_t {
	char *id;
	char *name;
	char *type;
	char *signature;
	bool enabled;
	struct port_t *inports;
	struct port_t *outports;
	managed_attributes_t *managed_attr;
	actor_state_t *state;
	result_t (*init_actor)(char *obj, actor_state_t **state);
	result_t (*fire)(struct actor_t *actor);
	void (*free_state)(struct actor_t *actor);
	char *(*serialize_state)(actor_state_t *state, char **buffer);
} actor_t;

result_t create_actor(struct node_t *node, char *root, actor_t **actor);
result_t disconnect_actor(struct node_t *node, actor_t *actor);
void free_actor(struct node_t *node, actor_t *actor, bool remove_from_storage);
char *serialize_actor(const actor_t *actor, char **buffer);
result_t add_actor(struct node_t *node, actor_t *actor);
void delete_actor(struct node_t *node, actor_t *actor, bool remove_from_storage);
actor_t *get_actor(struct node_t *node, const char *actor_id);
result_t connect_actor(struct node_t *node, actor_t *actor, tunnel_t *tunnel);
void enable_actor(actor_t *actor);
void disable_actor(actor_t *actor);

#endif /* ACTORS_H */