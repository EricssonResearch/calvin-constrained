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
#include "runtime/south/platform/cc_platform.h"
#include "coder/cc_coder.h"

void cc_token_set_data(cc_token_t *token, char *data, const size_t size)
{
	if (token->value != NULL) {
		cc_log_debug("Token not freed");
		cc_platform_mem_free(token->value);
		token->value = NULL;
	}

	token->value = data;
	token->size = size;
}

char *cc_token_encode(char *buffer, cc_token_t *token, bool with_key)
{
	if (with_key)
		buffer = cc_coder_encode_kv_map(buffer, "token", 2);
	else
		buffer = cc_coder_encode_map(buffer, 2);

	buffer = cc_coder_encode_kv_str(buffer, "type", "Token", 5);
	if (token->size != 0)
		buffer = cc_coder_encode_kv_value(buffer, "data", token->value, token->size);
	else
		buffer = cc_coder_encode_kv_nil(buffer, "data");

	return buffer;
}

void cc_token_free(cc_token_t *token)
{
	if (token->value != NULL) {
		cc_platform_mem_free(token->value);
		token->value = NULL;
		token->size = 0;
	}
}
