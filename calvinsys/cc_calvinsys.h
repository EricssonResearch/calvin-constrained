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

typedef struct calvinsys_t {
	char* name;
	char* data;
	char* command;
	bool new_data;
	size_t data_size;
	result_t (*init)(struct calvinsys_t* calvinsys);
	result_t (*release)(struct calvinsys_t* calvinsys);
	result_t (*write)(struct calvinsys_t* calvinsys, char* command, char* data, size_t data_size);
	struct node_t* node;
	void* state;
} calvinsys_t;

result_t register_calvinsys(struct node_t* node, struct calvinsys_t* calvinsys);
void unregister_calvinsys(struct node_t* node, calvinsys_t* calvinsys);
void release_calvinsys(calvinsys_t** calvinsys);
#endif
