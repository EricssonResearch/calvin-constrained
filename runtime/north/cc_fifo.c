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
#include "coder/cc_coder.h"
#include "runtime/south/platform/cc_platform.h"

cc_fifo_t *cc_fifo_init(char *obj_fifo, char* obj_properties)
{
	cc_result_t result = CC_SUCCESS;
	cc_fifo_t *fifo = NULL;
	char *reader = NULL, *tmp_reader = NULL, *obj_read_pos = NULL, *obj_tokens = NULL, *obj_readers = NULL;
	char *obj_token = NULL, *obj_tentative_read_pos = NULL, *obj_data = NULL, *queuetype = NULL, *r = obj_fifo;
	char *data = NULL, *p = obj_properties;
	uint32_t i_token = 0, queuetype_len = 0, reader_len = 0, nbr_of_tokens = 0, size = 0;

	if (cc_platform_mem_alloc((void **)&fifo, sizeof(cc_fifo_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	fifo->size = 0;
	fifo->write_pos = 0;
	fifo->read_pos = 0;
	fifo->tentative_read_pos = 0;

	if (obj_fifo != NULL && cc_coder_decode_string_from_map(r, "queuetype", &queuetype, &queuetype_len) != CC_SUCCESS)
		return NULL;

	if (obj_fifo != NULL && strncmp("fanout_fifo", queuetype, queuetype_len) != 0) {
		cc_log_error("Queue type not supported");
		return NULL;
	}

	if (obj_fifo != NULL && cc_coder_decode_uint_from_map(r, "N", &fifo->size) != CC_SUCCESS)
		return NULL;

	if (obj_fifo == NULL && cc_coder_decode_uint_from_map(p, "queue_length", &fifo->size) != CC_SUCCESS)
		fifo->size = 4;

	if (cc_platform_mem_alloc((void **)&fifo->tokens, sizeof(cc_token_t) * fifo->size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}
	for (i_token = 0; i_token < fifo->size; i_token++) {
		fifo->tokens[i_token].value = NULL;
		fifo->tokens[i_token].size = 0;
	}

	// When init without previous queue state done
	if (obj_fifo == NULL)
		return fifo;

	if (cc_coder_decode_uint_from_map(r, "write_pos", &fifo->write_pos) != CC_SUCCESS)
		return NULL;

	if (cc_coder_get_value_from_map(r, "tentative_read_pos", &obj_tentative_read_pos) != CC_SUCCESS)
		return NULL;

	if (cc_coder_get_value_from_map(r, "readers", &obj_readers) != CC_SUCCESS)
		return NULL;

	if (cc_coder_decode_string_from_array(obj_readers, 0, &reader, &reader_len) != CC_SUCCESS)
		return NULL;

	if (cc_platform_mem_alloc((void **)&tmp_reader, reader_len + 1) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	strncpy(tmp_reader, reader, reader_len);
	tmp_reader[reader_len] = '\0';

	if (result == CC_SUCCESS)
		result = cc_coder_decode_uint_from_map(obj_tentative_read_pos, tmp_reader, &fifo->tentative_read_pos);

	if (cc_coder_get_value_from_map(r, "read_pos", &obj_read_pos) != CC_SUCCESS)
		return NULL;

	if (result == CC_SUCCESS)
		result = cc_coder_decode_uint_from_map(obj_read_pos, tmp_reader, &fifo->read_pos);

	if (cc_coder_get_value_from_map(r, "fifo", &obj_tokens) == CC_SUCCESS) {
		nbr_of_tokens = cc_coder_get_size_of_array(obj_tokens);
		for (i_token = 0; i_token < nbr_of_tokens; i_token++) {
			if (cc_coder_get_value_from_array(obj_tokens, i_token, &obj_token) != CC_SUCCESS) {
				result = CC_FAIL;
				break;
			}

			if (cc_coder_get_value_from_map(obj_token, "data", &obj_data) != CC_SUCCESS) {
				result = CC_FAIL;
				break;
			}

			if (fifo->read_pos != fifo->tentative_read_pos || fifo->write_pos != fifo->read_pos) {
				size = cc_coder_get_size_of_value(obj_data);
				if (cc_platform_mem_alloc((void **)&data, size) != CC_SUCCESS) {
					cc_log_error("Failed to allocate memory");
					result = CC_FAIL;
					break;
				}
				memcpy(data, obj_data, size);
				cc_token_set_data(&fifo->tokens[i_token], data, size);
			}
		}
	}

	if (tmp_reader != NULL)
		cc_platform_mem_free((void *)tmp_reader);

	if (result != CC_SUCCESS)
		return NULL;

	return fifo;
}

void cc_fifo_free(cc_fifo_t *fifo)
{
	uint32_t i_token = 0;

	for (i_token = 0; i_token < fifo->size; i_token++)
		cc_token_free(&fifo->tokens[i_token]);
	cc_platform_mem_free((void *)fifo->tokens);
	fifo->size = 0;
	fifo->write_pos = 0;
	fifo->read_pos = 0;
	fifo->tentative_read_pos = 0;
	cc_platform_mem_free((void *)fifo);
}

void cc_fifo_cancel(cc_fifo_t *fifo)
{
	fifo->tentative_read_pos = fifo->read_pos;
}

cc_token_t *cc_fifo_peek(cc_fifo_t *fifo)
{
	uint32_t read_pos = 0;

	read_pos = fifo->tentative_read_pos;
	fifo->tentative_read_pos = read_pos + 1;
	return &fifo->tokens[read_pos % fifo->size];
}

void cc_fifo_commit_read(cc_fifo_t *fifo, bool free_token)
{
	if (fifo->read_pos < fifo->tentative_read_pos) {
		if (free_token)
			cc_token_free(&fifo->tokens[fifo->read_pos % fifo->size]);
		else {
			fifo->tokens[fifo->read_pos % fifo->size].value = NULL;
			fifo->tokens[fifo->read_pos % fifo->size].size = 0;
		}
		fifo->read_pos++;
	} else
		cc_log_error("Invalid commit");
}

void cc_fifo_cancel_commit(cc_fifo_t *fifo)
{
	fifo->tentative_read_pos = fifo->read_pos;
}

bool cc_fifo_slots_available(const cc_fifo_t *fifo, uint32_t length)
{
	return (fifo->size - ((fifo->write_pos - fifo->read_pos) % fifo->size) - 1) >= length;
}

bool cc_fifo_tokens_available(const cc_fifo_t *fifo, uint32_t length)
{
	return (fifo->write_pos - fifo->tentative_read_pos) >= length;
}

cc_result_t cc_fifo_write(cc_fifo_t *fifo, char *data, const size_t size)
{
	if (!cc_fifo_slots_available(fifo, 1))
		return CC_FAIL;

	cc_token_set_data(&fifo->tokens[fifo->write_pos % fifo->size], data, size);
	fifo->write_pos++;

	return CC_SUCCESS;
}

void cc_fifo_com_peek(cc_fifo_t *fifo, cc_token_t **token, uint32_t *sequence_nbr)
{
	*sequence_nbr = fifo->tentative_read_pos;
	*token = cc_fifo_peek(fifo);
}

cc_result_t cc_fifo_com_write(cc_fifo_t *fifo, char *data, size_t size, uint32_t sequence_nbr)
{
	if (sequence_nbr >= fifo->write_pos) { // TODO: Should be sequence_nbr == fifo->write_pos
		cc_fifo_write(fifo, data, size);
		return CC_SUCCESS;
	}

	cc_log_error("Unhandled token with sequencenbr '%ld' with write_pos '%ld'", (unsigned long)sequence_nbr, (unsigned long)fifo->write_pos);

	return CC_SUCCESS;
}

void cc_fifo_com_commit_read(cc_fifo_t *fifo, uint32_t sequence_nbr)
{
	if (sequence_nbr >= fifo->tentative_read_pos)
		return;

	if (fifo->read_pos < fifo->tentative_read_pos) {
		if (sequence_nbr == fifo->read_pos) {
			cc_token_free(&fifo->tokens[fifo->read_pos % fifo->size]);
			fifo->read_pos++;
			return;
		}
	}

	cc_log_error("Unhandled commit");
}

void cc_fifo_com_cancel_read(cc_fifo_t *fifo, uint32_t sequence_nbr)
{
	if (sequence_nbr >= fifo->tentative_read_pos && sequence_nbr < fifo->read_pos) {
		cc_log_error("Invalid cancel");
		return;
	}
	fifo->tentative_read_pos = sequence_nbr;
}
