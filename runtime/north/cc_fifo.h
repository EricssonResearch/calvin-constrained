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
#include "cc_common.h"
#include "cc_token.h"

typedef struct fifo_t {
	uint32_t size;
	uint32_t write_pos;
	uint32_t read_pos;
	uint32_t tentative_read_pos;
	token_t *tokens;
} fifo_t;

fifo_t *fifo_init(char *obj_fifo);
void fifo_free(fifo_t *fifo);
void fifo_cancel(fifo_t *fifo);
token_t *fifo_peek(fifo_t *fifo);
void fifo_commit_read(fifo_t *fifo, bool free_token);
void fifo_cancel_commit(fifo_t *fifo);
bool fifo_slots_available(const fifo_t *fifo, uint32_t length);
bool fifo_tokens_available(const fifo_t *fifo, uint32_t length);
result_t fifo_write(fifo_t *fifo, char *data, const size_t size);
void fifo_com_peek(fifo_t *fifo, token_t **token, uint32_t *sequence_nbr);
result_t fifo_com_write(fifo_t *fifo, char *data, size_t size, uint32_t sequence_nbr);
void fifo_com_commit_read(fifo_t *fifo, uint32_t sequence_nbr);
void fifo_com_cancel_read(fifo_t *fifo, uint32_t sequence_nbr);

#endif /* FIFO_H */
