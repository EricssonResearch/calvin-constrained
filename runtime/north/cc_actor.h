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
#include "cc_config.h"
#include "cc_common.h"
#include "cc_port.h"
#include "runtime/south/platform/cc_platform.h"
#include "calvinsys/cc_calvinsys.h"

struct cc_node_t;
struct cc_actor_type_t;

typedef enum {
	CC_ACTOR_PENDING,
	CC_ACTOR_PENDING_IMPL,
	CC_ACTOR_ENABLED,
	CC_ACTOR_DO_DELETE
} cc_actor_state_t;

typedef enum {
	CC_STATE_WAS_ACTOR,
	CC_STATE_WAS_SHADOW,
	CC_STATE_WAS_REPLICA
} cc_state_was_t;

typedef struct cc_actor_t{
	char *type;
	char *id;
	char *name;
	cc_state_was_t state_was;
	char migrate_to[CC_UUID_BUFFER_SIZE]; // id of rt to migrate to if requested
	cc_actor_state_t state;
	void *instance_state;
	cc_list_t *in_ports;
	cc_list_t *out_ports;
	cc_list_t *private_attributes;
	cc_list_t *managed_attributes;
	cc_result_t (*init)(struct cc_actor_t *actor, cc_list_t *managed_attributes);
	cc_result_t (*set_state)(struct cc_actor_t *actor, cc_list_t *managed_attributes);
	bool (*fire)(struct cc_actor_t *actor);
	void (*free_state)(struct cc_actor_t *actor);
	cc_result_t (*get_managed_attributes)(struct cc_actor_t *actor, cc_list_t **attributes);
	void (*will_migrate)(struct cc_actor_t *actor);
	void (*will_end)(struct cc_actor_t *actor);
	void (*did_migrate)(struct cc_actor_t *actor);
	void (*did_replicate)(struct cc_actor_t *actor, uint32_t index);
	cc_result_t (*get_requires)(struct cc_actor_t *actor, cc_list_t **requires);
	cc_calvinsys_t *calvinsys;
	char *requires;
} cc_actor_t;

cc_result_t cc_actor_req_match_reply_handler(struct cc_node_t *node, char *data, size_t data_len, void *msg_data);
cc_actor_t *cc_actor_create(struct cc_node_t *node, char *root);
cc_actor_t *cc_actor_create_from_type(struct cc_node_t *node, char *type, uint32_t type_len);
void cc_actor_free(struct cc_node_t *node, cc_actor_t *actor, bool remove_from_registry);
cc_actor_t *cc_actor_get(struct cc_node_t *node, const char *actor_id, uint32_t actor_id_len);
cc_actor_t *cc_actor_get_from_name(struct cc_node_t *node, const char *name, uint32_t name_len);
void cc_actor_port_state_changed(cc_actor_t *actor);
void cc_actor_disconnect(struct cc_node_t *node, cc_actor_t *actor, bool unref_tunnel);
void cc_actor_connect_ports(struct cc_node_t *node, cc_actor_t *actor);
cc_result_t cc_actor_migrate(struct cc_node_t *node, cc_actor_t *actor, char *to_rt_uuid, uint32_t to_rt_uuid_len);
char *cc_actor_serialize(const struct cc_node_t *node, cc_actor_t *actor, char *buffer, bool include_state);
void cc_actor_free_attribute_list(cc_list_t *managed_attributes);

#endif /* CC_ACTOR_H */
