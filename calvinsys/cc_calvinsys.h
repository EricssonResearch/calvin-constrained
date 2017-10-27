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
#ifndef CC_CALVINSYS_H
#define CC_CALVINSYS_H

#include "runtime/north/cc_common.h"

typedef struct cc_calvinsys_obj_t {
	bool (*can_write)(struct cc_calvinsys_obj_t *obj);
	cc_result_t (*write)(struct cc_calvinsys_obj_t *obj, char *data, size_t data_size);
	bool (*can_read)(struct cc_calvinsys_obj_t *obj);
	cc_result_t (*read)(struct cc_calvinsys_obj_t *obj, char **data, size_t *data_size);
	cc_result_t (*close)(struct cc_calvinsys_obj_t *obj);
	char *(*serialize)(char *id, struct cc_calvinsys_obj_t *obj, char *buffer);
	void *state;
	char *id;
	struct cc_actor_t *actor;
	struct cc_calvinsys_capability_t *capability;
} cc_calvinsys_obj_t;

typedef struct cc_calvinsys_capability_t {
	cc_result_t (*open)(cc_calvinsys_obj_t *obj, char *data, size_t len);
	cc_result_t (*deserialize)(cc_calvinsys_obj_t *obj, char *buffer);
	struct cc_calvinsys_t *calvinsys;
	void *state;
	char *name;
} cc_calvinsys_capability_t;

typedef struct cc_calvinsys_t {
	cc_list_t *capabilities;
	cc_list_t *objects;
	struct cc_node_t *node;
} cc_calvinsys_t;

cc_result_t cc_calvinsys_create_capability(cc_calvinsys_t *calvinsys, const char *name, cc_result_t (*open)(cc_calvinsys_obj_t*, char*, size_t), cc_result_t (*deserialize)(cc_calvinsys_obj_t*, char*), void *state);
void cc_calvinsys_delete_capability(cc_calvinsys_t *calvinsys, const char *name);
char *cc_calvinsys_open(struct cc_actor_t *actor, const char *name, char *data, size_t size);
bool cc_calvinsys_can_write(cc_calvinsys_t *calvinsys, char *id);
cc_result_t cc_calvinsys_write(cc_calvinsys_t *calvinsys, char *id, char *data, size_t data_size);
bool cc_calvinsys_can_read(cc_calvinsys_t *calvinsys, char *id);
cc_result_t cc_calvinsys_read(cc_calvinsys_t *calvinsys, char *id, char **data, size_t *data_size);
void cc_calvinsys_close(cc_calvinsys_t *calvinsys, char *id);
cc_result_t cc_calvinsys_get_attributes(cc_calvinsys_t *calvinsys, struct cc_actor_t *actor, cc_list_t **private_attributes);
cc_result_t cc_calvinsys_deserialize(struct cc_actor_t *actor, char *buffer);

#endif /* CC_CALVINSYS_H */
