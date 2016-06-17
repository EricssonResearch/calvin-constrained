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

result_t create_fifo(fifo_t **fifo, char *obj_fifo)
{
	result_t result = FAIL;
	char *reader = NULL, *obj_read_pos = NULL, *obj_tokens = NULL, *obj_readers = NULL;
	char *obj_token = NULL, *obj_tentative_read_pos = NULL, *queuetype = NULL, *r = obj_fifo;
	int i_token = 0;
	token_t *token = NULL;

	*fifo = (fifo_t*)malloc(sizeof(fifo_t));
	if (*fifo == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	(*fifo)->size = 0;
	(*fifo)->write_pos = 0;
	(*fifo)->read_pos = 0;
	(*fifo)->tentative_read_pos = 0;

	for (i_token = 0; i_token < MAX_TOKENS; i_token++) {
		(*fifo)->tokens[i_token] = NULL;
	}

	result = decode_string_from_map(&r, "queuetype", &queuetype);
	if (result == SUCCESS && strcmp("fanout_fifo", queuetype) != 0) {
		log_error("Queue type '%s' not supported", queuetype);
		result = FAIL;
	}

	if (result == SUCCESS) {
		result = get_value_from_map(&r, "readers", &obj_readers);
	}

	if (result == SUCCESS) {
		result = decode_string_from_array(&obj_readers, 0, &reader);
	}

	if (result == SUCCESS) {
		result = decode_uint_from_map(&r, "N", &(*fifo)->size);
	}

	if (result == SUCCESS) {
		result = decode_uint_from_map(&r, "write_pos", &(*fifo)->write_pos);
	}

	if (result == SUCCESS) {
		result = get_value_from_map(&r, "tentative_read_pos", &obj_tentative_read_pos);
	}

	if (result == SUCCESS) {
		result = decode_uint_from_map(&obj_tentative_read_pos, reader, &(*fifo)->tentative_read_pos);
	}

	if (result == SUCCESS) {
		result = get_value_from_map(&r, "read_pos", &obj_read_pos);
	}

	if (result == SUCCESS) {
		result = decode_uint_from_map(&obj_read_pos, reader, &(*fifo)->read_pos);
	}

	if (result == SUCCESS) {
		if ((*fifo)->tentative_read_pos != (*fifo)->write_pos) {
			result = get_value_from_map(&r, "fifo", &obj_tokens);
			if (result == SUCCESS) {
				for (i_token = ((*fifo)->read_pos % (*fifo)->size); i_token < ((*fifo)->write_pos % (*fifo)->size); i_token++) {
					result = get_value_from_array(&obj_tokens, i_token, &obj_token);
					if (result == SUCCESS) {
						result = decode_token(obj_token, &token);
						if (result != SUCCESS) {
							log_error("Failed to create token");
							break;
						}
						(*fifo)->tokens[i_token] = token;
					}
				}
			}
		}
	}

	if (result != SUCCESS) {
		free_fifo(*fifo);
	}

	if (reader != NULL) {
		free(reader);
	}

	if (queuetype != NULL) {
		free(queuetype);
	}

	return result;
}

void free_fifo(fifo_t *fifo)
{
	uint32_t i_token = 0;

	if (fifo != NULL) {
		for (i_token = 0; i_token < fifo->size; i_token++) {
			if (fifo->tokens[i_token] != NULL) {
				free_token(fifo->tokens[i_token]);
			}
		}
	}

	free(fifo);
}

bool fifo_can_read(fifo_t *fifo)
{
	return fifo->tentative_read_pos != fifo->write_pos;
}

token_t *fifo_read(fifo_t *fifo)
{
	uint32_t read_pos = 0;
	token_t *token = NULL;

	read_pos = fifo->tentative_read_pos;
	token = fifo->tokens[read_pos % fifo->size];
	fifo->tentative_read_pos = read_pos + 1;
	return token;
}

void fifo_commit_read(fifo_t* fifo, bool commit, bool delete_token)
{
	token_t *token = NULL;

	if (fifo != NULL) {
		if (commit) {
			token = fifo->tokens[fifo->read_pos % fifo->size];
			if (token != NULL) {
				if (delete_token) {
					free_token(token);
				}
				fifo->tokens[fifo->read_pos % fifo->size] = NULL;
				fifo->read_pos += 1;
			} else {
				log_error("Failed to get token at %ld", (unsigned long)fifo->read_pos);
			}
		} else {
			fifo->tentative_read_pos -= 1;
		}
	} else {
		log_error("fifo is NULL");
	}
}

bool fifo_can_write(fifo_t *fifo)
{
	return (fifo->write_pos + 1) % fifo->size != fifo->read_pos % fifo->size;
}

result_t fifo_write(fifo_t *fifo, token_t *token)
{
    if (!fifo_can_write(fifo)) {
    	return FAIL;
    }

    if (fifo->tokens[fifo->write_pos % fifo->size] != NULL) {
    	free_token(fifo->tokens[fifo->write_pos % fifo->size]);
    }

    fifo->tokens[fifo->write_pos % fifo->size] = token;
    fifo->write_pos++;
    return SUCCESS;
}