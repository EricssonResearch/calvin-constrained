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

#include "../runtime/north/cc_common.h"

typedef struct cc_calvinsys_obj_t {
	bool (*can_write)(struct cc_calvinsys_obj_t *obj);
	cc_result_t (*write)(struct cc_calvinsys_obj_t *obj, char *data, size_t data_size);
	bool (*can_read)(struct cc_calvinsys_obj_t *obj);
	cc_result_t (*read)(struct cc_calvinsys_obj_t *obj, char **data, size_t *data_size);
	cc_result_t (*close)(struct cc_calvinsys_obj_t *obj);
	void *state;
	uint32_t id;
	struct cc_calvinsys_handler_t *handler;
	struct cc_calvinsys_obj_t *next;
} cc_calvinsys_obj_t;

typedef struct cc_calvinsys_handler_t {
	cc_calvinsys_obj_t *(*open)(struct cc_calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char *capability_name);
	cc_calvinsys_obj_t *objects;
	struct cc_calvinsys_handler_t *next;
	struct cc_calvinsys_t *calvinsys;
} cc_calvinsys_handler_t;

typedef struct cc_calvinsys_capability_t {
	cc_calvinsys_handler_t *handler;
	void *state;
} cc_calvinsys_capability_t;

typedef struct cc_calvinsys_t {
	cc_list_t *capabilities;
	cc_calvinsys_handler_t *handlers;
	uint32_t next_id;
	struct cc_node_t *node;
} cc_calvinsys_t;

void cc_calvinsys_add_handler(cc_calvinsys_t **calvinsys, cc_calvinsys_handler_t *handler);
void cc_calvinsys_delete_handler(cc_calvinsys_handler_t *handler);
cc_result_t cc_calvinsys_register_capability(cc_calvinsys_t *calvinsys, const char *name, cc_calvinsys_handler_t *handler, void *state);
void cc_calvinsys_delete_capabiltiy(cc_calvinsys_t *calvinsys, const char *name);
cc_calvinsys_obj_t *cc_calvinsys_open(cc_calvinsys_t *calvinsys, const char *name, char *data, size_t size);
void cc_calvinsys_close(cc_calvinsys_obj_t *obj);
cc_result_t cc_calvinsys_get_obj_by_id(cc_calvinsys_obj_t **obj, cc_calvinsys_handler_t *handler, uint32_t id);

#endif /* CC_CALVINSYS_H */
