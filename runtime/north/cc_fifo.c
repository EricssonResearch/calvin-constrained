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
#include <stdlib.h>
#include <string.h>
#include "cc_fifo.h"
#include "cc_msgpack_helper.h"
#include "../south/platform/cc_platform.h"
#include "../../msgpuck/msgpuck.h"

result_t fifo_init(fifo_t *fifo, char *obj_fifo)
{
	result_t result = CC_RESULT_SUCCESS;
	char *reader = NULL, *tmp_reader = NULL, *obj_read_pos = NULL, *obj_tokens = NULL, *obj_readers = NULL;
	char *obj_token = NULL, *obj_tentative_read_pos = NULL, *obj_data = NULL, *queuetype = NULL, *r = obj_fifo;
	uint32_t i_token = 0, queuetype_len = 0, reader_len = 0, nbr_of_tokens = 0, size = 0;

	fifo->size = 0;
	fifo->write_pos = 0;
	fifo->read_pos = 0;
	fifo->tentative_read_pos = 0;
	for (i_token = 0; i_token < MAX_TOKENS; i_token++) {
		fifo->tokens[i_token].value = NULL;
		fifo->tokens[i_token].size = 0;
	}

	if (decode_string_from_map(r, "queuetype", &queuetype, &queuetype_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (strncmp("fanout_fifo", queuetype, queuetype_len) != 0) {
		log_error("Queue type '%.*s' not supported", (int)queuetype_len, queuetype);
		return CC_RESULT_FAIL;
	}

	if (get_value_from_map(r, "readers", &obj_readers) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_string_from_array(obj_readers, 0, &reader, &reader_len) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_uint_from_map(r, "N", &fifo->size) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (decode_uint_from_map(r, "write_pos", &fifo->write_pos) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (get_value_from_map(r, "tentative_read_pos", &obj_tentative_read_pos) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (get_value_from_map(r, "read_pos", &obj_read_pos) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	if (platform_mem_alloc((void **)&tmp_reader, reader_len + 1) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	strncpy(tmp_reader, reader, reader_len);
	tmp_reader[reader_len] = '\0';

	if (result == CC_RESULT_SUCCESS)
		result = decode_uint_from_map(obj_tentative_read_pos, tmp_reader, &fifo->tentative_read_pos);

	if (result == CC_RESULT_SUCCESS)
		result = decode_uint_from_map(obj_read_pos, tmp_reader, &fifo->read_pos);

	if (get_value_from_map(r, "fifo", &obj_tokens) == CC_RESULT_SUCCESS) {
		nbr_of_tokens = get_size_of_array(obj_tokens);
		for (i_token = 0; i_token < nbr_of_tokens; i_token++) {
			if (get_value_from_array(obj_tokens, i_token, &obj_token) != CC_RESULT_SUCCESS) {
				result = CC_RESULT_FAIL;
				break;
			}

			if (get_value_from_map(obj_token, "data", &obj_data) != CC_RESULT_SUCCESS) {
				result = CC_RESULT_FAIL;
				break;
			}

			if (fifo->read_pos != fifo->tentative_read_pos || fifo->write_pos != fifo->read_pos) {
				size = get_size_of_value(obj_data);
				if (platform_mem_alloc((void **)&fifo->tokens[i_token].value, size) != CC_RESULT_SUCCESS) {
					log_error("Failed to allocate memory");
					result = CC_RESULT_FAIL;
					break;
				}
				memcpy(fifo->tokens[i_token].value, obj_data, size);
				fifo->tokens[i_token].size = size;
			}
		}
	}

	if (result != CC_RESULT_SUCCESS)
		fifo_free(fifo);

	if (tmp_reader != NULL)
		platform_mem_free((void *)tmp_reader);

	return result;
}

void fifo_free(fifo_t *fifo)
{
	fifo->size = 0;
	fifo->write_pos = 0;
	fifo->read_pos = 0;
	fifo->tentative_read_pos = 0;
	memset(fifo->tokens, 0, MAX_TOKENS * sizeof(token_t));
}

void fifo_cancel(fifo_t *fifo)
{
	fifo->tentative_read_pos = fifo->read_pos;
}

token_t *fifo_peek(fifo_t *fifo)
{
	uint32_t read_pos = 0;

	read_pos = fifo->tentative_read_pos;
	fifo->tentative_read_pos = read_pos + 1;
	return &fifo->tokens[read_pos % fifo->size];
}

void fifo_commit_read(fifo_t *fifo)
{
	fifo->read_pos = fifo->tentative_read_pos;
}

void fifo_cancel_commit(fifo_t *fifo)
{
	fifo->tentative_read_pos = fifo->read_pos;
}

bool fifo_slots_available(const fifo_t *fifo, uint32_t length)
{
	return (fifo->size - ((fifo->write_pos - fifo->read_pos) % fifo->size) - 1) >= length;
}

bool fifo_tokens_available(const fifo_t *fifo, uint32_t length)
{
	return (fifo->write_pos - fifo->tentative_read_pos) >= length;
}

result_t fifo_write(fifo_t *fifo, char *data, const size_t size)
{
	result_t result = CC_RESULT_SUCCESS;

	if (!fifo_slots_available(fifo, 1))
		return CC_RESULT_FAIL;

	token_set_data(&fifo->tokens[fifo->write_pos % fifo->size], data, size);
	fifo->write_pos++;

	return result;
}

void fifo_com_peek(fifo_t *fifo, token_t **token, uint32_t *sequence_nbr)
{
	*sequence_nbr = fifo->tentative_read_pos;
	*token = fifo_peek(fifo);
}

result_t fifo_com_write(fifo_t *fifo, char *data, size_t size, uint32_t sequence_nbr)
{
	if (sequence_nbr >= fifo->write_pos) { // TODO: Should be sequence_nbr == fifo->write_pos
		fifo_write(fifo, data, size);
		return CC_RESULT_SUCCESS;
	}

	log_error("Unhandled token with sequencenbr '%ld' with write_pos '%ld'", (unsigned long)sequence_nbr, (unsigned long)fifo->write_pos);

	return CC_RESULT_FAIL;
}

void fifo_com_commit_read(fifo_t *fifo, uint32_t sequence_nbr)
{
	if ((sequence_nbr < fifo->tentative_read_pos) && (fifo->read_pos < fifo->tentative_read_pos)) {
		while (fifo->read_pos <= sequence_nbr) {
			token_free(&fifo->tokens[sequence_nbr % fifo->size]);
			fifo->tokens[sequence_nbr % fifo->size].value = NULL;
			fifo->tokens[sequence_nbr % fifo->size].size = 0;
			fifo->read_pos += 1;
		}
		return;
	}

	log_error("Invalid commit");
}

void fifo_com_cancel_read(fifo_t *fifo, uint32_t sequence_nbr)
{
	if (sequence_nbr >= fifo->tentative_read_pos && sequence_nbr < fifo->read_pos) {
		log_error("Invalid cancel");
		return;
	}
	fifo->tentative_read_pos = sequence_nbr;
}
