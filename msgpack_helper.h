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
#ifndef MSGPACK_HELPER_H
#define MSGPACK_HELPER_H

#include <stdbool.h>
#include "common.h"

char *encode_str(char **buffer, const char *key, const char *value);
char *encode_map(char **buffer, const char *key, int keys);
char *encode_array(char **buffer, const char *key, int keys);
char *encode_uint(char **buffer, const char *key, uint32_t value);
char *encode_double(char **buffer, const char *key, double value);
char *encode_bool(char **buffer, const char *key, bool value);
char *encode_nil(char **buffer, const char *key);
char *encode_value(char **buffer, const char *key, const char *value, size_t size);
bool has_key(char **buffer, const char *key);
result_t get_value_from_array(char **buffer, int index, char **value);
result_t get_value_from_map(char **buffer, const char *key, char **value);
result_t decode_str(char **buffer, char **value);
result_t decode_string_from_map(char **buffer, const char *key, char **value);
result_t decode_string_from_array(char **buffer, int index, char **value);
result_t decode_uint_from_map(char **buffer, const char *key, uint32_t *value);
result_t decode_bool_from_map(char **buffer, const char *key, bool *value);
result_t decode_double_from_map(char **buffer, const char *key, double *value);
result_t copy_value(char **buffer, const char *key, char **value, size_t *size);

#endif /* MSGPACK_HELPER_H */
