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
#include "../../msgpuck/msgpuck.h"
#include "cc_msgpack_helper.h"
#include "../south/platform/cc_platform.h"

char *encode_str(char **buffer, const char *key, const char *value, uint32_t value_len)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_str(*buffer, value, value_len);
	return *buffer;
}

char *encode_bin(char **buffer, const char *key, const char *value, uint32_t value_len)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_bin(*buffer, value, value_len);
	return *buffer;
}

char *encode_uint(char **buffer, const char *key, uint32_t value)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_uint(*buffer, value);
	return *buffer;
}

char *encode_int(char **buffer, const char *key, int32_t value)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_int(*buffer, value);
	return *buffer;
}

char *encode_double(char **buffer, const char *key, double value)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_double(*buffer, value);
	return *buffer;
}

char *encode_float(char **buffer, const char *key, float value)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_float(*buffer, value);
	return *buffer;
}

char *encode_bool(char **buffer, const char *key, bool value)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_bool(*buffer, value);
	return *buffer;
}

char *encode_nil(char **buffer, const char *key)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_nil(*buffer);
	return *buffer;
}

char *encode_map(char **buffer, const char *key, int keys)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_map(*buffer, keys);
	return *buffer;
}

char *encode_array(char **buffer, const char *key, int keys)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_array(*buffer, keys);
	return *buffer;
}

char *encode_value(char **buffer, const char *key, const char *value, size_t size)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	memcpy(*buffer, value, size);
	*buffer += size;
	return *buffer;
}

bool has_key(char *buffer, const char *key)
{
	char *r = buffer, *tmp = NULL;
	uint32_t i = 0, map_size = 0, key_len = 0;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(r, &tmp, &key_len) == SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, key_len) == 0)
					return true;
			}
			mp_next((const char **)&r);
		}
	} else
		log_error("Buffer is not a map");

	return false;
}

uint32_t get_size_of_array(char *buffer)
{
	char *r = buffer;

	return mp_decode_array((const char **)&r);
}

result_t get_value_from_array(char *buffer, int index, char **value)
{
	char *r = buffer;
	uint32_t i = 0, size = mp_decode_array((const char **)&r);

	if (index < size) {
		for (i = 0; i < index; i++)
			mp_next((const char **)&r);
		*value = r;
		return SUCCESS;
	}

	log_error("Parse error for index '%d'", index);

	return FAIL;
}

result_t get_value_from_map_n(char *buffer, const char *key, uint32_t key_len, char **value)
{
	char *r = buffer;
	uint32_t i = 0, map_size = 0, tmp_key_len = 0;
	char *tmp_key = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(r, &tmp_key, &tmp_key_len) == SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(key, tmp_key, key_len) == 0) {
					*value = r;
					return SUCCESS;
				}
				mp_next((const char **)&r);
			} else {
				log_error("Failed to decode map");
				return FAIL;
			}
		}
	} else
		log_error("Buffer is not a map");

	log_error("Parse error for '%s'", key);

	return FAIL;
}

result_t get_value_from_map(char *buffer, const char *key, char **value)
{
		return get_value_from_map_n(buffer, key, strlen(key), value);
}

result_t decode_str(char *buffer, char **value, uint32_t *len)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_STR) {
		log_error("Failed to decode value, not a string");
		return FAIL;
	}

	*value = (char *)mp_decode_str((const char **)&r, len);

	return SUCCESS;
}

result_t decode_bin(char *buffer, char **value, uint32_t *len)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_BIN) {
		log_error("Failed to decode value, not binary data");
		return FAIL;
	}

	*value = (char *)mp_decode_bin((const char **)&r, len);

	return SUCCESS;
}

result_t decode_bool(char *buffer, bool *value)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_BOOL) {
		log_error("Failed to decode value, not a boolean");
		return FAIL;
	}

	*value = mp_decode_bool((const char **)&r);

	return SUCCESS;
}

result_t decode_uint(char *buffer, uint32_t *value)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_UINT) {
		log_error("Failed to decode value, not a uint");
		return FAIL;
	}

	*value = mpk_decode_uint((const char **)&r);

	return SUCCESS;
}

result_t decode_int(char *buffer, int32_t *value)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_INT) {
		log_error("Failed to decode value, not a int");
		return FAIL;
	}

	*value = mp_decode_int((const char **)&r);

	return SUCCESS;
}

result_t decode_float(char *buffer, float *value)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_FLOAT) {
		log_error("Failed to decode value, not a float");
		return FAIL;
	}

	*value = mp_decode_float((const char **)&r);

	return SUCCESS;
}

result_t decode_double(char *buffer, double *value)
{
	char *r = buffer;

	if (mp_typeof(*r) != MP_DOUBLE) {
		log_error("Failed to decode value, not a double");
		return FAIL;
	}

	*value = mp_decode_double((const char **)&r);

	return SUCCESS;
}

result_t decode_string_from_map(char *buffer, const char *key, char **value, uint32_t *len)
{
	char *r = buffer, *tmp = NULL;
	uint32_t i = 0, map_size = 0, str_len = 0;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(r, &tmp, &str_len) == SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, str_len) == 0)
					return decode_str(r, value, len);
			}
			mp_next((const char **)&r);
		}
	} else {
		log_error("Buffer is not a map");
		return FAIL;
	}

	log_error("Parse error for '%s'", key);

	return FAIL;
}

result_t decode_bin_from_map(char *buffer, const char *key, char **value, uint32_t *len)
{
	char *r = buffer, *tmp = NULL;
	uint32_t i = 0, map_size = 0, str_len = 0;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(r, &tmp, &str_len) == SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, str_len) == 0)
					return decode_bin(r, value, len);
			}
			mp_next((const char **)&r);
		}
	} else {
		log_error("Buffer is not a map");
		return FAIL;
	}

	log_error("Parse error for '%s'", key);

	return FAIL;
}

result_t decode_string_from_array(char *buffer, int index, char **value, uint32_t *len)
{
	char *r = buffer;
	uint32_t i = 0, size = mp_decode_array((const char **)&r);

	if (index < size) {
		for (i = 0; i < index; i++)
			mp_next((const char **)&r);
		return decode_str(r, value, len);
	}

	log_error("Index out of range");

	return FAIL;
}

result_t decode_uint_from_map(char *buffer, const char *key, uint32_t *value)
{
	char *r = buffer;
	uint32_t i = 0, map_size = 0, key_len = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(r, &tmp, &key_len) == SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, key_len) == 0) {
					*value = mpk_decode_uint((const char **)&r);
					return SUCCESS;
				}
				mp_next((const char **)&r);
			}
		}

	} else
		log_error("Buffer is not a map");

	log_error("Parse error for '%s'", key);

	return FAIL;
}

result_t decode_bool_from_map(char *buffer, const char *key, bool *value)
{
	char *r = buffer;
	uint32_t i = 0, map_size = 0, key_len = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(r, &tmp, &key_len) == SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, key_len) == 0) {
					*value = mp_decode_bool((const char **)&r);
					return SUCCESS;
				}
				mp_next((const char **)&r);
			}
		}
	} else
		log_error("Buffer is not a map");

	log_error("Parse error for '%s'", key);

	return FAIL;
}

result_t decode_double_from_map(char *buffer, const char *key, double *value)
{
	char *r = buffer;
	uint32_t i = 0, map_size = 0, key_len = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(r, &tmp, &key_len) == SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, key_len) == 0) {
					*value = mp_decode_double((const char **)&r);
					return SUCCESS;
				}
				mp_next((const char **)&r);
			}
		}

	} else
		log_error("Buffer is not a map");

	log_error("Parse error for '%s'", key);

	return FAIL;
}

result_t decode_float_from_map(char *buffer, const char *key, float *value)
{
	char *r = buffer;
	uint32_t i = 0, map_size = 0, key_len = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(r, &tmp, &key_len) == SUCCESS) {
				mp_next((const char **)&r);
				if (strncmp(tmp, key, key_len) == 0) {
					*value = mp_decode_float((const char **)&r);
					return SUCCESS;
				}
				mp_next((const char **)&r);
			}
		}

	} else
		log_error("Buffer is not a map");

	log_error("Parse error for '%s'", key);
	return FAIL;
}

size_t get_size_of_value(char *value)
{
	char *next = value;

	mp_next((const char **)&next);

	return next - value;
}
