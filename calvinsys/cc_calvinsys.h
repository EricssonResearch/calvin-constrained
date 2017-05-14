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

struct node_t;

typedef struct calvinsys_obj_t {
	// write messagepack encode data
	result_t (*write)(struct calvinsys_obj_t *obj, char *data, size_t data_size);
	// data/device ready
	bool (*can_read)(struct calvinsys_obj_t *obj);
	// return data in messagepack format
	result_t (*read)(struct calvinsys_obj_t *obj, char **data, size_t *data_size);
	// close the object
	result_t (*close)(struct calvinsys_obj_t *obj);
	struct calvinsys_handler_t *handler;
	struct calvinsys_obj_t *next;
} calvinsys_obj_t;

typedef struct calvinsys_handler_t {
	calvinsys_obj_t *(*open)(struct calvinsys_handler_t *handler, char *data, size_t len);
	calvinsys_obj_t *objects;
} calvinsys_handler_t;

result_t calvinsys_register_handler(list_t **calvinsys, const char *name, calvinsys_handler_t *handler);
void calvinsys_free_handler(struct node_t *node, char *name);
calvinsys_obj_t *calvinsys_open(list_t *calvinsys, const char *name, char *data, size_t size);
void calvinsys_close(calvinsys_obj_t *obj);

#endif /* CALVINSYS_H */
