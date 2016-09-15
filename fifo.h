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
#ifndef FIFO_H
#define FIFO_H

#include <stdbool.h>
#include "common.h"
#include "token.h"

#define MAX_TOKENS 5

typedef struct fifo_t {
	token_t *tokens[MAX_TOKENS];
	uint32_t size;
	uint32_t write_pos;
	uint32_t read_pos;
	uint32_t tentative_read_pos;
} fifo_t;

result_t create_fifo(fifo_t **fifo, char *obj_fifo);
void free_fifo(fifo_t *fifo);
bool fifo_can_read(const fifo_t *fifo);
token_t *fifo_read(fifo_t *fifo);
void fifo_commit_read(fifo_t *fifo, bool commit, bool delete_token);
bool fifo_can_write(const fifo_t *fifo);
result_t fifo_write(fifo_t *fifo, token_t *token);

#endif /* FIFO_H */