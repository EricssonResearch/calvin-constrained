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
#ifndef CC_FIFO_H
#define CC_FIFO_H

#include <stdbool.h>
#include "cc_common.h"
#include "cc_token.h"

typedef struct cc_fifo_t {
	uint32_t size;
	uint32_t write_pos;
	uint32_t read_pos;
	uint32_t tentative_read_pos;
	cc_token_t *tokens;
} cc_fifo_t;

cc_fifo_t *cc_fifo_init(char *obj_fifo);
void cc_fifo_free(cc_fifo_t *fifo);
void cc_fifo_cancel(cc_fifo_t *fifo);
cc_token_t *cc_fifo_peek(cc_fifo_t *fifo);
void cc_fifo_commit_read(cc_fifo_t *fifo, bool free_token);
void cc_fifo_cancel_commit(cc_fifo_t *fifo);
bool cc_fifo_slots_available(const cc_fifo_t *fifo, uint32_t length);
bool cc_fifo_tokens_available(const cc_fifo_t *fifo, uint32_t length);
cc_result_t cc_fifo_write(cc_fifo_t *fifo, char *data, const size_t size);
void cc_fifo_com_peek(cc_fifo_t *fifo, cc_token_t **token, uint32_t *sequence_nbr);
cc_result_t cc_fifo_com_write(cc_fifo_t *fifo, char *data, size_t size, uint32_t sequence_nbr);
void cc_fifo_com_commit_read(cc_fifo_t *fifo, uint32_t sequence_nbr);
void cc_fifo_com_cancel_read(cc_fifo_t *fifo, uint32_t sequence_nbr);

#endif /* CC_FIFO_H */
