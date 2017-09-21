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
#ifndef CC_ACTOR_H
#define CC_ACTOR_H

#include <stdbool.h>
#include "cc_common.h"
#include "cc_port.h"
#include "../south/platform/cc_platform.h"
#include "../../calvinsys/cc_calvinsys.h"

struct cc_node_t;

typedef enum {
	CC_ACTOR_PENDING,
	CC_ACTOR_ENABLED
} cc_actor_state_t;

typedef struct cc_actor_t{
	char id[CC_UUID_BUFFER_SIZE];
	char name[CC_UUID_BUFFER_SIZE];
	char type[CC_UUID_BUFFER_SIZE];
	char migrate_to[CC_UUID_BUFFER_SIZE]; // id of rt to migrate to if requested
	cc_actor_state_t state;
	void *instance_state;
	cc_list_t *in_ports;
	cc_list_t *out_ports;
	cc_list_t *attributes;
	cc_result_t (*init)(struct cc_actor_t **actor, cc_list_t *attributes);
	cc_result_t (*set_state)(struct cc_actor_t **actor, cc_list_t *attributes);
	bool (*fire)(struct cc_actor_t *actor);
	void (*free_state)(struct cc_actor_t *actor);
	cc_result_t (*get_managed_attributes)(struct cc_actor_t *actor, cc_list_t **attributes);
	void (*will_migrate)(struct cc_actor_t *actor);
	void (*will_end)(struct cc_actor_t *actor);
	void (*did_migrate)(struct cc_actor_t *actor);
	cc_calvinsys_t *calvinsys;
} cc_actor_t;

cc_actor_t *cc_actor_create(struct cc_node_t *node, char *root);
void cc_actor_free(struct cc_node_t *node, cc_actor_t *actor, bool remove_from_registry);
cc_actor_t *cc_actor_get(struct cc_node_t *node, const char *actor_id, uint32_t actor_id_len);
void cc_actor_CC_PORT_ENABLED(cc_actor_t *actor);
void cc_actor_CC_PORT_DISCONNECTED(cc_actor_t *actor);
void cc_actor_disconnect(struct cc_node_t *node, cc_actor_t *actor);
cc_result_t cc_actor_migrate(struct cc_node_t *node, cc_actor_t *actor, char *to_rt_uuid, uint32_t to_rt_uuid_len);
char *cc_actor_serialize(const struct cc_node_t *node, const cc_actor_t *actor, char *buffer, bool include_state);

#endif /* CC_ACTOR_H */
