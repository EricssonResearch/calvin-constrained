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
#ifndef CC_TOKEN_H
#define CC_TOKEN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cc_common.h"

typedef struct cc_token_t {
	char *value;
	size_t size;
} cc_token_t;

void cc_token_set_data(cc_token_t *token, char *data, const size_t size);
char *cc_token_encode(char *buffer, cc_token_t *token, bool with_key);
void cc_token_free(cc_token_t *token);

#endif /* CC_TOKEN_H */
