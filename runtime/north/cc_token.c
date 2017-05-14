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
#include "cc_token.h"
#include "cc_common.h"
#include "../south/platform/cc_platform.h"
#include "cc_msgpack_helper.h"
#include "../../msgpuck/msgpuck.h"

void token_set_data(token_t *token, char *data, const size_t size)
{
	token->value = data;
	token->size = size;
}

char *token_encode(char **buffer, token_t token, bool with_key)
{
	if (with_key)
		*buffer = encode_map(buffer, "token", 2);
	else
		*buffer = mp_encode_map(*buffer, 2);

	*buffer = encode_str(buffer, "type", "Token", strlen("Token"));
	if (token.size != 0)
		*buffer = encode_value(buffer, "data", token.value, token.size);
	else
		*buffer = encode_nil(buffer, "data");

	return *buffer;
}

void token_set_double(token_t *token, const double value)
{
	token->size = mp_sizeof_double(value);
	if (platform_mem_alloc((void **)&(token->value), token->size) == CC_RESULT_SUCCESS)
		mp_encode_double(token->value, value);
	else
		log_error("Failed to allocate memory");
}

void token_set_uint(token_t *token, const uint32_t value)
{
	token->size = mp_sizeof_uint(value);
	if (platform_mem_alloc((void **)&(token->value), token->size) == CC_RESULT_SUCCESS)
		mp_encode_uint(token->value, value);
	else
		log_error("Failed to allocate memory");
}

result_t token_decode_uint(token_t token, uint32_t *value)
{
	char *data = token.value;

	if (mp_typeof(*data) == MP_UINT) {
		*value = mpk_decode_uint((const char **)&data);
		return CC_RESULT_SUCCESS;
	}

	return CC_RESULT_FAIL;
}

void token_free(token_t *token)
{
	if (token->value != NULL && token->size != 0) {
		platform_mem_free((void *)token->value);
		token->value = NULL;
		token->size = 0;
	}
}
