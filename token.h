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
#ifndef TOKEN_H
#define TOKEN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common.h"

typedef struct token_t {
	char value[MAX_TOKEN_SIZE];	// Serialized token data
	size_t size;				// Size of serialized token data
} token_t;

result_t token_set_data(token_t *token, const char *data, const size_t size);
char *token_encode(char **buffer, token_t token, bool with_key);
void token_set_double(token_t *token, const double value);
void token_set_uint(token_t *token, const uint32_t value);
result_t token_decode_uint(token_t token, uint32_t *value);

#endif /* TOKEN_H */
