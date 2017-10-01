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
#ifndef CC_CODER_H
#define CC_CODER_H

#include <stdbool.h>
#include "../cc_common.h"

typedef enum {
  CC_CODER_UNDEF,
  CC_CODER_FLOAT,
  CC_CODER_DOUBLE,
  CC_CODER_INT,
  CC_CODER_UINT,
  CC_CODER_STR,
  CC_CODER_BOOL,
  CC_CODER_ARRAY,
  CC_CODER_BIN,
  CC_CODER_MAP,
  CC_CODER_NIL
} cc_coder_type_t;

cc_coder_type_t cc_coder_type_of(char *buffer);
bool cc_coder_has_key(char *buffer, const char *key);
uint32_t cc_coder_sizeof_bool(bool value);
uint32_t cc_coder_sizeof_uint(uint32_t value);
uint32_t cc_coder_sizeof_int(int32_t value);
uint32_t cc_coder_sizeof_double(double value);
uint32_t cc_coder_sizeof_float(float value);
uint32_t cc_coder_sizeof_str(uint32_t len);
uint32_t cc_coder_sizeof_nil(void);
size_t cc_coder_get_size_of_value(char *value);
uint32_t cc_coder_get_size_of_array(char *buffer);
char *cc_coder_encode_map(char *buffer, uint32_t items);
char *cc_coder_encode_str(char *buffer, const char *data, uint32_t len);
char *cc_coder_encode_uint(char *buffer, uint32_t data);
char *cc_coder_encode_int(char *buffer, uint32_t data);
char *cc_coder_encode_double(char *buffer, double data);
char *cc_coder_encode_float(char *buffer, float data);
char *cc_coder_encode_nil(char *buffer);
char *cc_coder_encode_array(char *buffer, uint32_t items);
char *cc_coder_encode_bool(char *buffer, bool value);
char *cc_coder_encode_kv_str(char *buffer, const char *key, const char *value, uint32_t value_len);
char *cc_coder_encode_kv_map(char *buffer, const char *key, int keys);
char *cc_coder_encode_kv_array(char *buffer, const char *key, int keys);
char *cc_coder_encode_kv_bin(char *buffer, const char *key, const char *value, uint32_t value_len);
char *cc_coder_encode_kv_uint(char *buffer, const char *key, uint32_t value);
char *cc_coder_encode_kv_int(char *buffer, const char *key, int32_t value);
char *cc_coder_encode_kv_double(char *buffer, const char *key, double value);
char *cc_coder_encode_kv_float(char *buffer, const char *key, float value);
char *cc_coder_encode_kv_bool(char *buffer, const char *key, bool value);
char *cc_coder_encode_kv_nil(char *buffer, const char *key);
char *cc_coder_encode_kv_value(char *buffer, const char *key, const char *value, size_t size);
cc_result_t cc_coder_get_value_from_array(char *buffer, uint32_t index, char **value);
cc_result_t cc_coder_get_value_from_map_n(char *buffer, const char *key, uint32_t key_len, char **value);
cc_result_t cc_coder_get_value_from_map(char *buffer, const char *key, char **value);
cc_result_t cc_coder_decode_bool(char *buffer, bool *value);
cc_result_t cc_coder_decode_uint(char *buffer, uint32_t *value);
cc_result_t cc_coder_decode_bin(char *buffer, char **value, uint32_t *len);
cc_result_t cc_coder_decode_int(char *buffer, int32_t *value);
cc_result_t cc_coder_decode_float(char *buffer, float *value);
cc_result_t cc_coder_decode_double(char *buffer, double *value);
cc_result_t cc_coder_decode_str(char *buffer, char **value, uint32_t *len);
cc_result_t cc_coder_decode_string_from_map(char *buffer, const char *key, char **value, uint32_t *len);
cc_result_t cc_coder_decode_bin_from_map(char *buffer, const char *key, char **value, uint32_t *len);
cc_result_t cc_coder_decode_string_from_array(char *buffer, uint32_t index, char **value, uint32_t *len);
cc_result_t cc_coder_decode_uint_from_map(char *buffer, const char *key, uint32_t *value);
cc_result_t cc_coder_decode_bool_from_map(char *buffer, const char *key, bool *value);
cc_result_t cc_coder_decode_float_from_map(char *buffer, const char *key, float *value);
cc_result_t cc_coder_decode_double_from_map(char *buffer, const char *key, double *value);
cc_result_t cc_coder_copy_value(char *buffer, const char *key, char **value, size_t *size);
uint32_t cc_coder_decode_map(char **data);
void cc_coder_decode_map_next(char **data);
char *cc_coder_get_name(void);

#endif /* CC_CODER_H */
