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
#ifndef CALVINSYS_H
#define CALVINSYS_H

#include "../runtime/north/cc_common.h"

typedef struct calvinsys_obj_t {
	bool (*can_write)(struct calvinsys_obj_t *obj);
	result_t (*write)(struct calvinsys_obj_t *obj, char *data, size_t data_size);
	bool (*can_read)(struct calvinsys_obj_t *obj);
	result_t (*read)(struct calvinsys_obj_t *obj, char **data, size_t *data_size);
	result_t (*close)(struct calvinsys_obj_t *obj);
	void *state;
	uint32_t id;
	struct calvinsys_handler_t *handler;
	struct calvinsys_obj_t *next;
} calvinsys_obj_t;

typedef struct calvinsys_handler_t {
	calvinsys_obj_t *(*open)(struct calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char *capability_name);
	calvinsys_obj_t *objects;
	struct calvinsys_handler_t *next;
	struct calvinsys_t *calvinsys;
} calvinsys_handler_t;

typedef struct calvinsys_capability_t {
	calvinsys_handler_t *handler;
	void *state;
} calvinsys_capability_t;

typedef struct calvinsys_t {
	list_t *capabilities;
	calvinsys_handler_t *handlers;
	uint32_t next_id;
	struct node_t *node;
} calvinsys_t;

void calvinsys_add_handler(calvinsys_t **calvinsys, calvinsys_handler_t *handler);
void calvinsys_delete_handler(calvinsys_handler_t *handler);
result_t calvinsys_register_capability(calvinsys_t *calvinsys, const char *name, calvinsys_handler_t *handler, void *state);
void calvinsys_delete_capabiltiy(calvinsys_t *calvinsys, const char *name);
calvinsys_obj_t *calvinsys_open(calvinsys_t *calvinsys, const char *name, char *data, size_t size);
void calvinsys_close(calvinsys_obj_t *obj);
result_t calvinsys_get_obj_by_id(calvinsys_obj_t **obj, calvinsys_handler_t *handler, uint32_t id);

#endif /* CALVINSYS_H */
