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
#include "msgpack_helper.h"
#include "platform.h"

char *encode_str(char **buffer, const char *key, const char *value)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_str(*buffer, value, strlen(value));
	return *buffer;
}

char *encode_uint(char **buffer, const char *key, uint32_t value)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_uint(*buffer, value);
	return *buffer;
}

char *encode_double(char **buffer, const char *key, double value)
{
	*buffer = mp_encode_str(*buffer, key, strlen(key));
	*buffer = mp_encode_double(*buffer, value);
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

bool has_key(char **buffer, const char *key)
{
	char *r = *buffer, *tmp = NULL;
	uint32_t i = 0, map_size = 0;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(&r, &tmp) == SUCCESS) {
				if (strncmp(tmp, key, strlen(key)) == 0) {
					free(tmp);
					return true;
				}
				free(tmp);
			}
			mp_next((const char **)&r);
		}
	} else {
		log_error("Buffer is not a map");
	}

	return false;
}

result_t get_value_from_array(char **buffer, int index, char **value)
{
	char *r = *buffer;
	uint32_t i = 0, size = mp_decode_array((const char **)&r);

	if (index < size) {
		for (i = 0; i < index; i++)
			mp_next((const char **)&r);
		*value = r;
		return SUCCESS;
	}

	log_error("Failed to get index '%d'", index);

	return FAIL;
}

result_t get_value_from_map(char **buffer, const char *key, char **value)
{
	char *r = *buffer;
	uint32_t i = 0, map_size = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(&r, &tmp) == SUCCESS) {
				if (strncmp(tmp, key, strlen(key)) == 0) {
					free(tmp);
					*value = r;
					return SUCCESS;
				}
				free(tmp);
				mp_next((const char **)&r);
			} else {
				log_error("Failed to decode map");
				return FAIL;
			}
		}
	} else {
		log_error("Buffer is not a map");
	}

	log_error("Failed to get '%s'", key);

	return FAIL;
}

result_t decode_str(char **buffer, char **value)
{
	uint32_t len;
	const char *tmp = NULL;

	if (mp_typeof(**buffer) != MP_STR) {
		*value = NULL;
		return SUCCESS;
	}

	tmp = mp_decode_str((const char **)buffer, &len);

	*value = (char *)malloc(len + 1);
	if (*value != NULL) {
		memcpy(*value, tmp, len);
		(*value)[len] = '\0';
		return SUCCESS;
	}

	log_error("Failed to allocate memory");
	return FAIL;
}

result_t decode_string_from_map(char **buffer, const char *key, char **value)
{
	char *r = *buffer, *tmp = NULL;
	uint32_t i = 0, map_size = 0;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(&r, &tmp) == SUCCESS) {
				if (strncmp(tmp, key, strlen(key)) == 0) {
					free(tmp);
					return decode_str(&r, value);
				}
				free(tmp);
			}
			mp_next((const char **)&r);
		}
	} else {
		log_error("Buffer is not a map");
	}

	log_error("Failed to get '%s'", key);

	return FAIL;
}

result_t decode_string_from_array(char **buffer, int index, char **value)
{
	char *r = *buffer;
	uint32_t i = 0, size = mp_decode_array((const char **)&r);

	if (index < size) {
		for (i = 0; i < index; i++)
			mp_next((const char **)&r);
		return decode_str(&r, value);
	}

	log_error("Index out of range");

	return FAIL;
}

result_t decode_uint_from_map(char **buffer, const char *key, uint32_t *value)
{
	char *r = *buffer;
	uint32_t i = 0, map_size = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(&r, &tmp) == SUCCESS) {
				if (strncmp(tmp, key, strlen(key)) == 0) {
					free(tmp);
					*value = mp_decode_uint((const char **)&r);
					return SUCCESS;
				}
				mp_next((const char **)&r);
				free(tmp);
			}
		}

	} else {
		log_error("Buffer is not a map");
	}

	log_error("Failed to get '%s'", key);

	return FAIL;
}

result_t decode_bool_from_map(char **buffer, const char *key, bool *value)
{
	char *r = *buffer;
	uint32_t i = 0, map_size = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(&r, &tmp) == SUCCESS) {
				if (strncmp(tmp, key, strlen(key)) == 0) {
					free(tmp);
					*value = mp_decode_bool((const char **)&r);
					return SUCCESS;
				}
				mp_next((const char **)&r);
				free(tmp);
			}
		}
	} else {
		log_error("Buffer is not a map");
	}

	log_error("Failed to get '%s'", key);

	return FAIL;
}

result_t decode_double_from_map(char **buffer, const char *key, double *value)
{
	char *r = *buffer;
	uint32_t i = 0, map_size = 0;
	char *tmp = NULL;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(&r, &tmp) == SUCCESS) {
				if (strncmp(tmp, key, strlen(key)) == 0) {
					free(tmp);
					*value = mp_decode_double((const char **)&r);
					return SUCCESS;
				}
				mp_next((const char **)&r);
				free(tmp);
			}
		}

	} else {
		log_error("Buffer is not a map");
	}

	log_error("Failed to get '%s'", key);

	return FAIL;
}

result_t copy_value(char **buffer, const char *key, char **value, size_t *size)
{
	char *r = *buffer, *tmp = NULL, *start = NULL;
	uint32_t i = 0, map_size = 0;

	if (mp_typeof(*r) == MP_MAP) {
		map_size = mp_decode_map((const char **)&r);
		for (i = 0; i < map_size; i++) {
			if (decode_str(&r, &tmp) == SUCCESS) {
				if (strncmp(tmp, key, strlen(key)) == 0) {
					free(tmp);
					start = r;
					mp_next((const char **)&r);
					*size = r - start;
					*value = malloc(*size);
					if (*value == NULL) {
						log_error("Failed to allocate memory");
						return FAIL;
					}
					memcpy(*value, start, *size);
					return SUCCESS;
				}
				free(tmp);
			}
			mp_next((const char **)&r);
		}
	} else {
		log_error("Buffer is not a map");
	}

	log_error("Failed to get '%s'", key);

	return FAIL;
}
