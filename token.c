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
#include "token.h"
#include "common.h"
#include "platform.h"
#include "msgpack_helper.h"
#include "msgpuck/msgpuck.h"

result_t create_double_token(double value, token_t **token)
{
	*token = (token_t *)malloc(sizeof(token_t));
	if (*token == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	(*token)->size = mp_sizeof_double(value);

	(*token)->value = malloc((*token)->size);
	if ((*token)->value == NULL) {
		log_error("Failed to allocate memory");
		free(*token);
		return FAIL;
	}

	mp_encode_double((*token)->value, value);

	return SUCCESS;
}

result_t create_uint_token(uint32_t value, token_t **token)
{
	*token = (token_t *)malloc(sizeof(token_t));
	if (*token == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	(*token)->size = mp_sizeof_uint(value);

	(*token)->value = malloc((*token)->size);
	if ((*token)->value == NULL) {
		log_error("Failed to allocate memory");
		free(*token);
		return FAIL;
	}

	mp_encode_uint((*token)->value, value);

	return SUCCESS;
}

result_t decode_token(char *obj_token, token_t **token)
{
	*token = (token_t *)malloc(sizeof(token_t));
	if (*token == NULL) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	if (copy_value(&obj_token, "data", &(*token)->value, &(*token)->size) == SUCCESS)
		return SUCCESS;

	log_error("Failed to copy token");

	return FAIL;
}

char *encode_token(char **buffer, token_t *token, bool with_key)
{
	if (with_key)
		*buffer = encode_map(buffer, "token", 2);
	else
		*buffer = mp_encode_map(*buffer, 2);

	*buffer = encode_str(buffer, "type", "Token");
	if (token != NULL && token->value != NULL)
		*buffer = encode_value(buffer, "data", token->value, token->size);
	else
		*buffer = encode_nil(buffer, "data");

	return *buffer;
}

void free_token(token_t *token)
{
	if (token != NULL) {
		if (token->value != NULL)
			free(token->value);
		free(token);
	}
}

result_t decode_uint_token(const token_t *token, uint32_t *out)
{
	char *value = NULL;

	if (token != NULL && token->value != NULL) {
		value = token->value;
		if (mp_typeof(*value) == MP_UINT) {
			*out = mp_decode_uint((const char **)&value);
			return SUCCESS;
		}
		else
			log_error("Unsupported type");
	} else
		log_error("NULL token");

	return FAIL;
}
