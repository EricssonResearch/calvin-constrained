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

typedef enum {
	ACTOR_DO_DELETE,
	ACTOR_DO_ENABLE,
	ACTOR_DO_MIGRATE,
	ACTOR_ENABLED,
	ACTOR_PENDING
} actor_state_t;

typedef struct actor_t {
	char id[UUID_BUFFER_SIZE];
	char name[UUID_BUFFER_SIZE];
	char type[UUID_BUFFER_SIZE];
	char migrate_to[UUID_BUFFER_SIZE]; // id of rt to migrate to if requested
	actor_state_t state;
	void *instance_state;
	list_t *in_ports;
	list_t *out_ports;
	result_t (*init)(struct actor_t **actor, char *obj_actor_state);
	result_t (*set_state)(struct actor_t **actor, char *obj_actor_state);
	result_t (*fire)(struct actor_t *actor);
	void (*free_state)(struct actor_t *actor);
	char *(*serialize_state)(struct actor_t *actor, char **buffer);
	list_t *(*get_managed_attributes)(struct actor_t *actor);
} actor_t;

actor_t *actor_create(struct node_t *node, char *root);
void actor_free(struct node_t *node, actor_t *actor);
void actor_free_managed(list_t *managed_attributes);
result_t actor_get_managed(char *obj_actor_state, list_t **managed_attributes);
actor_t *actor_get(struct node_t *node, const char *actor_id, uint32_t actor_id_len);
void actor_port_enabled(actor_t *actor);
void actor_delete(actor_t *actor);
result_t actor_migrate(actor_t *actor, char *to_rt_uuid, uint32_t to_rt_uuid_len);
char *actor_serialize_managed_list(list_t *managed_attributes, char **buffer);
char *actor_serialize(const actor_t *actor, char **buffer);
result_t actor_transmit(struct node_t *node, actor_t *actor);

#endif /* ACTORS_H */
