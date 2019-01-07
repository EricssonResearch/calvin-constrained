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
#include "msgpuck/msgpuck.h"
#include "cc_coder.h"
#include "runtime/north/cc_common.h"
#include "runtime/south/platform/cc_platform.h"

cc_coder_type_t cc_coder_type_of(char *buffer)
{
	cc_coder_type_t type;

	switch (mp_typeof(*buffer)) {
		case MP_FLOAT:
			type = CC_CODER_FLOAT;
			break;
		case MP_DOUBLE:
			type = CC_CODER_DOUBLE;
			break;
		case MP_BOOL:
			type = CC_CODER_BOOL;
			break;
		case MP_INT:
			type = CC_CODER_INT;
			break;
		case MP_UINT:
			type = CC_CODER_UINT;
			break;
		case MP_MAP:
			type = CC_CODER_MAP;
			break;
		case MP_STR:
			type = CC_CODER_STR;
			break;
		case MP_NIL:
			type = CC_CODER_NIL;
			break;
		case MP_BIN:
			type = CC_CODER_BIN;
			break;
		case MP_ARRAY:
			type = CC_CODER_ARRAY;
			break;
		default:
			type = CC_CODER_UNDEF;
			break;
	}

	return type;
}

uint32_t cc_coder_sizeof_uint(uint32_t value)
{
	return mp_sizeof_uint(value);
}

uint32_t cc_coder_sizeof_int(int32_t value)
{
	return mp_sizeof_int(value);
}

uint32_t cc_coder_sizeof_bool(bool value)
{
	return mp_sizeof_bool(value);
}

uint32_t cc_coder_sizeof_double(double value)
{
	return mp_sizeof_double(value);
}

uint32_t cc_coder_sizeof_float(float value)
{
	return mp_sizeof_float(value);
}

uint32_t cc_coder_sizeof_str(uint32_t len)
{
	return mp_sizeof_str(len) + len;
}

uint32_t cc_coder_sizeof_nil(void)
{
	return mp_sizeof_nil();
}

char *cc_coder_encode_map(char *buffer, uint32_t items)
{
	return mp_encode_map(buffer, items);
}

char *cc_coder_encode_str(char *buffer, const char *data, uint32_t len)
{
	return mp_encode_str(buffer, data, len);
}

char *cc_coder_encode_bin(char *buffer, const char *data, uint32_t len)
{
	return mp_encode_bin(buffer, data, len);
}

char *cc_coder_encode_uint(char *buffer, uint32_t data)
{
	return mp_encode_uint(buffer, data);
}

char *cc_coder_encode_int(char *buffer, uint32_t data)
{
	return mp_encode_int(buffer, data);
}

char *cc_coder_encode_double(char *buffer, double data)
{
	return mp_encode_double(buffer, data);
}

char *cc_coder_encode_float(char *buffer, float data)
{
	return mp_encode_float(buffer, data);
}

char *cc_coder_encode_nil(char *buffer)
{
	return mp_encode_nil(buffer);
}

char *cc_coder_encode_array(char *buffer, uint32_t items)
{
	return mp_encode_array(buffer, items);
}

char *cc_coder_encode_bool(char *buffer, bool value)
{
	return mp_encode_bool(buffer, value);
}

char *cc_coder_encode_kv_str(char *buffer, const char *key, const char *value, uint32_t value_len)
{
	buffer = mp_encode_str(buffer, key, strlen(key));
	return mp_encode_str(buffer, value, value_len);
}

char *cc_coder_encode_kv_bin(char *buffer, const char *key, const char *value, uint32_t value_len)
{
	buffer = mp_encode_str(buffer, key, strlen(key));
	return mp_encode_bin(buffer, value, value_len);
}

char *cc_coder_encode_kv_uint(char *buffer, const char *key, uint32_t value)
{
	buffer = mp_encode_str(buffer, key, strlen(key));
	return mp_encode_uint(buffer, value);
}

char *cc_coder_encode_kv_int(char *buffer, const char *key, int32_t value)
{
	buffer = mp_encode_str(buffer, key, strlen(key));
	return mp_encode_int(buffer, value);
}

char *cc_coder_encode_kv_double(char *buffer, const char *key, double value)
{
	buffer = mp_encode_str(buffer, key, strlen(key));
	return mp_encode_double(buffer, value);
}

char *cc_coder_encode_kv_float(char *buffer, const char *key, float value)
{
	buffer = mp_encode_str(buffer, key, strlen(key));
	return mp_encode_float(buffer, value);
}

char *cc_coder_encode_kv_bool(char *buffer, const char *key, bool value)
{
	buffer = mp_encode_str(buffer, key, strlen(key));
	return mp_encode_bool(buffer, value);
}

char *cc_coder_encode_kv_nil(char *buffer, const char *key)
{
	buffer = mp_encode_str(buffer, key, strlen(key));
	return mp_encode_nil(buffer);
}

char *cc_coder_encode_kv_map(char *buffer, const char *key, int keys)
{
	buffer = mp_encode_str(buffer, key, strlen(key));
	return mp_encode_map(buffer, keys);
}

char *cc_coder_encode_kv_array(char *buffer, const char *key, int keys)
{
	buffer = mp_encode_str(buffer, key, strlen(key));
	return mp_encode_array(buffer, keys);
}

char *cc_coder_encode_kv_value(char *buffer, const char *key, const char *value, size_t size)
{
	buffer = mp_encode_str(buffer, key, strlen(key));
	if (value == NULL)
		buffer = cc_coder_encode_nil(buffer);
	else {
		memcpy(buffer, value, size);
		buffer += size;
	}

	return buffer;
}

bool cc_coder_has_key(char *buffer, const char *key)
{
	char *r = buffer, *tmp = NULL;
	uint32_t i = 0, map_size = 0, key_len = 0;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (cc_coder_decode_str(r, &tmp, &key_len) == CC_SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, key_len) == 0)
					return true;
			}
			mp_next((const char **)&r);
		}
	}

	return false;
}

uint32_t cc_coder_get_size_of_array(char *buffer)
{
	char *r = buffer;

	return mp_decode_array((const char **)&r);
}

cc_result_t cc_coder_get_value_from_array(char *buffer, uint32_t index, char **value)
{
	char *r = buffer;
	uint32_t i = 0, size = mp_decode_array((const char **)&r);

	if (index < size) {
		for (i = 0; i < index; i++)
			mp_next((const char **)&r);
		*value = r;
		return CC_SUCCESS;
	}

	return CC_FAIL;
}

cc_result_t cc_coder_get_value_from_map_n(char *buffer, const char *key, uint32_t key_len, char **value)
{
	char *r = buffer;
	uint32_t i = 0, map_size = 0, tmp_key_len = 0;
	char *tmp_key = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (cc_coder_decode_str(r, &tmp_key, &tmp_key_len) == CC_SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(key, tmp_key, key_len) == 0) {
					*value = r;
					return CC_SUCCESS;
				}
				mp_next((const char **)&r);
			} else
				return CC_FAIL;
		}
	}

	return CC_FAIL;
}

cc_result_t cc_coder_get_value_from_map(char *buffer, const char *key, char **value)
{
		return cc_coder_get_value_from_map_n(buffer, key, strlen(key), value);
}

cc_result_t cc_coder_decode_str(char *buffer, char **value, uint32_t *len)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_STR)
		return CC_FAIL;

	*value = (char *)mp_decode_str((const char **)&r, len);

	return CC_SUCCESS;
}

cc_result_t cc_coder_decode_bin(char *buffer, char **value, uint32_t *len)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_BIN)
		return CC_FAIL;

	*value = (char *)mp_decode_bin((const char **)&r, len);

	return CC_SUCCESS;
}

cc_result_t cc_coder_decode_bool(char *buffer, bool *value)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_BOOL)
		return CC_FAIL;

	*value = mp_decode_bool((const char **)&r);

	return CC_SUCCESS;
}

cc_result_t cc_coder_decode_uint(char *buffer, uint32_t *value)
{
	char *r = buffer;

	switch (mp_typeof(*r)) {
	case MP_UINT:
	{
		*value = mpk_decode_uint((const char **)&r);
		return CC_SUCCESS;
	}
	case MP_FLOAT:
	{
		float f;
		f = mp_decode_float((const char **)&r);
		*value = (uint32_t)f;
		return CC_SUCCESS;
	}
	case MP_DOUBLE:
	{
		double d;
		d = mp_decode_double((const char **)&r);
		*value = (uint32_t)d;
		return CC_SUCCESS;
	}
	default:
		cc_log_error("Unknown type %d", mp_typeof(*r));
	}

	return CC_FAIL;
}

cc_result_t cc_coder_decode_int(char *buffer, int32_t *value)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_INT)
		return CC_FAIL;

	*value = mp_decode_int((const char **)&r);

	return CC_SUCCESS;
}

cc_result_t cc_coder_decode_float(char *buffer, float *value)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_FLOAT)
		return CC_FAIL;

	*value = mp_decode_float((const char **)&r);

	return CC_SUCCESS;
}

cc_result_t cc_coder_decode_double(char *buffer, double *value)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_DOUBLE)
		return CC_FAIL;

	*value = mp_decode_double((const char **)&r);

	return CC_SUCCESS;
}

cc_result_t cc_coder_decode_string_from_map(char *buffer, const char *key, char **value, uint32_t *len)
{
	char *r = buffer, *tmp = NULL;
	uint32_t i = 0, map_size = 0, str_len = 0;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (cc_coder_decode_str(r, &tmp, &str_len) == CC_SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, str_len) == 0)
					return cc_coder_decode_str(r, value, len);
			}
			mp_next((const char **)&r);
		}
	}

	return CC_FAIL;
}

cc_result_t cc_coder_decode_bin_from_map(char *buffer, const char *key, char **value, uint32_t *len)
{
	char *r = buffer, *tmp = NULL;
	uint32_t i = 0, map_size = 0, str_len = 0;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (cc_coder_decode_str(r, &tmp, &str_len) == CC_SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, str_len) == 0)
					return cc_coder_decode_bin(r, value, len);
			}
			mp_next((const char **)&r);
		}
	}

	return CC_FAIL;
}

cc_result_t cc_coder_decode_string_from_array(char *buffer, uint32_t index, char **value, uint32_t *len)
{
	char *r = buffer;
	uint32_t i = 0, size = mp_decode_array((const char **)&r);

	if (index < size) {
		for (i = 0; i < index; i++)
			mp_next((const char **)&r);
		return cc_coder_decode_str(r, value, len);
	}

	return CC_FAIL;
}

cc_result_t cc_coder_decode_uint_from_map(char *buffer, const char *key, uint32_t *value)
{
	char *r = buffer;
	uint32_t i = 0, map_size = 0, key_len = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (cc_coder_decode_str(r, &tmp, &key_len) == CC_SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, key_len) == 0) {
					*value = mpk_decode_uint((const char **)&r);
					return CC_SUCCESS;
				}
				mp_next((const char **)&r);
			}
		}
	}

	return CC_FAIL;
}

cc_result_t cc_coder_decode_bool_from_map(char *buffer, const char *key, bool *value)
{
	char *r = buffer;
	uint32_t i = 0, map_size = 0, key_len = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (cc_coder_decode_str(r, &tmp, &key_len) == CC_SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, key_len) == 0) {
					*value = mp_decode_bool((const char **)&r);
					return CC_SUCCESS;
				}
				mp_next((const char **)&r);
			}
		}
	}

	return CC_FAIL;
}

cc_result_t cc_coder_decode_double_from_map(char *buffer, const char *key, double *value)
{
	char *r = buffer;
	uint32_t i = 0, map_size = 0, key_len = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (cc_coder_decode_str(r, &tmp, &key_len) == CC_SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, key_len) == 0) {
					*value = mp_decode_double((const char **)&r);
					return CC_SUCCESS;
				}
				mp_next((const char **)&r);
			}
		}
	}

	return CC_FAIL;
}

cc_result_t cc_coder_decode_float_from_map(char *buffer, const char *key, float *value)
{
	char *r = buffer;
	uint32_t i = 0, map_size = 0, key_len = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (cc_coder_decode_str(r, &tmp, &key_len) == CC_SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, key_len) == 0) {
					*value = mp_decode_float((const char **)&r);
					return CC_SUCCESS;
				}
				mp_next((const char **)&r);
			}
		}
	}

	return CC_FAIL;
}

size_t cc_coder_get_size_of_value(char *value)
{
	char *next = value;

	mp_next((const char **)&next);

	return next - value;
}

uint32_t cc_coder_decode_map(char **data)
{
	return mp_decode_map((const char **)data);
}

void cc_coder_decode_map_next(char **data)
{
	mp_next((const char **)data);
}

uint32_t cc_coder_decode_array(char **data)
{
	return mp_decode_array((const char **)data);
}

void cc_coder_decode_array_next(char **data)
{
	mp_next((const char **)data);
}

char *cc_coder_get_name(void)
{
	char *name = NULL;

	if (cc_platform_mem_alloc((void **)&name, 8) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	strcpy(name, "msgpack");

	return name;
}
