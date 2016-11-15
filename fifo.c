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
#include "fifo.h"
#include "common.h"
#include "platform.h"
#include "msgpack_helper.h"

result_t fifo_init(fifo_t *fifo, char *obj_fifo)
{
	result_t result = SUCCESS;
	char *reader = NULL, *tmp_reader = NULL, *obj_read_pos = NULL, *obj_tokens = NULL, *obj_readers = NULL;
	char *obj_token = NULL, *obj_tentative_read_pos = NULL, *obj_data = NULL, *queuetype = NULL, *r = obj_fifo;
	int i_token = 0;
	uint32_t queuetype_len = 0, reader_len = 0, nbr_of_tokens = 0;

	fifo->size = 0;
	fifo->write_pos = 0;
	fifo->read_pos = 0;
	fifo->tentative_read_pos = 0;
	for (i_token = 0; i_token < MAX_TOKENS; i_token++)
		memset(&fifo->tokens[i_token], 0, sizeof(token_t));

	if (decode_string_from_map(r, "queuetype", &queuetype, &queuetype_len) != SUCCESS)
		return FAIL;

	if (strncmp("fanout_fifo", queuetype, queuetype_len) != 0) {
		log_error("Queue type '%.*s' not supported", (int)queuetype_len, queuetype);
		return FAIL;
	}

	if (get_value_from_map(r, "readers", &obj_readers) != SUCCESS)
		return FAIL;

	if (decode_string_from_array(obj_readers, 0, &reader, &reader_len) != SUCCESS)
		return FAIL;

	if (decode_uint_from_map(r, "N", &fifo->size) != SUCCESS)
		return FAIL;

	if (decode_uint_from_map(r, "write_pos", &fifo->write_pos) != SUCCESS)
		return FAIL;

	if (get_value_from_map(r, "tentative_read_pos", &obj_tentative_read_pos) != SUCCESS)
		return FAIL;

	if (get_value_from_map(r, "read_pos", &obj_read_pos) != SUCCESS)
		return FAIL;

	if (get_value_from_map(r, "fifo", &obj_tokens) != SUCCESS)
		return FAIL;

	nbr_of_tokens = get_size_of_array(obj_tokens);
	for (i_token = 0; i_token < nbr_of_tokens; i_token++) {
		if (get_value_from_array(obj_tokens, i_token, &obj_token) != SUCCESS)
			return FAIL;

		if (get_value_from_map(obj_token, "data", &obj_data) != SUCCESS)
			return FAIL;

		if (token_set_data(&fifo->tokens[i_token], obj_data, get_size_of_value(obj_data)) != SUCCESS) {
			log_error("Failed to set token");
			return FAIL;
		}
	}

	if (platform_mem_alloc((void **)&tmp_reader, reader_len + 1) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	strncpy(tmp_reader, reader, reader_len);
	tmp_reader[reader_len] = '\0';

	if (result == SUCCESS)
		result = decode_uint_from_map(obj_tentative_read_pos, tmp_reader, &fifo->tentative_read_pos);

	if (result == SUCCESS)
		result = decode_uint_from_map(obj_read_pos, tmp_reader, &fifo->read_pos);

	if (result != SUCCESS)
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
//	log_debug("fifo_slots_available write_pos: %ld read_pos: %ld", (unsigned long)fifo->write_pos, (unsigned long)fifo->read_pos);
	return (fifo->size - ((fifo->write_pos - fifo->read_pos) % fifo->size) - 1) >= length;
}

bool fifo_tokens_available(const fifo_t *fifo, uint32_t length)
{
//	log_debug("fifo_tokens_available: write_pos: %ld  tentative_read_pos: %ld", (unsigned long)fifo->write_pos, (unsigned long)fifo->read_pos);
	return (fifo->write_pos - fifo->tentative_read_pos) >= length;
}

result_t fifo_write(fifo_t *fifo, const char *data, const size_t size)
{
	result_t result = SUCCESS;

	log_debug("fifo_write");
	if (fifo_slots_available(fifo, 1) != 1)
		return FAIL;

	result = token_set_data(&fifo->tokens[fifo->write_pos % fifo->size], data, size);
	if (result == SUCCESS)
		fifo->write_pos++;

	return SUCCESS;
}

void fifo_com_peek(fifo_t *fifo, token_t **token, uint32_t *sequence_nbr)
{
	*sequence_nbr = fifo->tentative_read_pos;
	*token = fifo_peek(fifo);
}

result_t fifo_com_write(fifo_t *fifo, const char *data, size_t size, uint32_t sequence_nbr)
{
	log_debug("fifo_com_write");
	if (sequence_nbr >= fifo->write_pos) { // TODO: Should be sequence_nbr == fifo->write_pos
		fifo_write(fifo, data, size);
		return SUCCESS;
	}

	log_error("Unhandled token with sequencenbr '%ld' with write_pos '%ld'", (unsigned long)sequence_nbr, (unsigned long)fifo->write_pos);

	return FAIL;
}

void fifo_com_commit_read(fifo_t *fifo, uint32_t sequence_nbr)
{
	if (sequence_nbr >= fifo->tentative_read_pos) {
		log_error("Invalid commit");
		return;
	}
	if (fifo->read_pos < fifo->tentative_read_pos) {
		if (sequence_nbr == fifo->read_pos)
			fifo->read_pos += 1;
		else
			log_error("Invalid commit");
	}
}

void fifo_com_cancel_read(fifo_t *fifo, uint32_t sequence_nbr)
{
	if (sequence_nbr >= fifo->tentative_read_pos && sequence_nbr < fifo->read_pos) {
		log_error("Invalid cancel");
		return;
	}
	fifo->tentative_read_pos = sequence_nbr;
}
