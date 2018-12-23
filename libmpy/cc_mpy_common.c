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
#include <string.h>
#include "cc_mpy_common.h"
#include "runtime/north/coder/cc_coder.h"
#include "py/objlist.h"

cc_result_t cc_mpy_decode_to_mpy_obj(char *buffer, mp_obj_t *value)
{
	cc_result_t result = CC_SUCCESS;
	char *tmp_string = NULL;
	uint32_t len = 0, u_num = 0;
	int32_t num = 0;
	bool b = false;
	float f = 0;
	double d = 0;
	mp_obj_t array_value = NULL;

	switch (cc_coder_type_of(buffer)) {
	case CC_CODER_NIL:
		*value = mp_const_none;
		return CC_SUCCESS;
	case CC_CODER_STR:
		result = cc_coder_decode_str(buffer, &tmp_string, &len);
		if (result == CC_SUCCESS) {
			*value = mp_obj_new_str(tmp_string, len);
			return CC_SUCCESS;
		}
		break;
	case CC_CODER_UINT:
		result = cc_coder_decode_uint(buffer, &u_num);
		if (result == CC_SUCCESS) {
			*value = mp_obj_new_int_from_uint(u_num);
			return CC_SUCCESS;
		}
		break;
	case CC_CODER_INT:
		result = cc_coder_decode_int(buffer, &num);
		if (result == CC_SUCCESS) {
			*value = mp_obj_new_int(num);
			return CC_SUCCESS;
		}
		break;
	case CC_CODER_BOOL:
		result = cc_coder_decode_bool(buffer, &b);
		if (result == CC_SUCCESS) {
			*value = mp_obj_new_bool(b);
			return CC_SUCCESS;
		}
		break;
	case CC_CODER_FLOAT:
		result = cc_coder_decode_float(buffer, &f);
		if (result == CC_SUCCESS) {
#if CC_PYTHON_FLOATS
			*value = mp_obj_new_float(f);
#else
			*value = mp_obj_new_int((int)f);
#endif
			return CC_SUCCESS;
		}
		break;
	case CC_CODER_DOUBLE:
		result = cc_coder_decode_double(buffer, &d);
		if (result == CC_SUCCESS) {
#if CC_PYTHON_FLOATS
			*value = mp_obj_new_float(d);
#else
			*value = mp_obj_new_int((int)d);
#endif
			return CC_SUCCESS;
		}
		break;
	case CC_CODER_ARRAY:
		len = cc_coder_get_size_of_array(buffer);
		*value = mp_obj_new_list(len, NULL);
		for (u_num = 0; u_num < len && result == CC_SUCCESS; u_num++) {
			if ((result = cc_coder_get_value_from_array(buffer, u_num, &tmp_string)) != CC_SUCCESS)
				cc_log_error("Failed to decode array item '%ld'", u_num);
			result = cc_mpy_decode_to_mpy_obj(tmp_string, &array_value);
			if (result == CC_SUCCESS)
				mp_obj_list_store(*value, MP_OBJ_NEW_SMALL_INT(u_num), array_value);
			else
				cc_log_error("Failed to decode object");
		}
		break;
	default:
		cc_log_error("Unsupported type");
		result = CC_FAIL;
	}

	return result;
}

cc_result_t cc_mpy_encode_from_mpy_obj(mp_obj_t input, char **buffer, size_t *size, bool alloc)
{
	char *pos = NULL;
	size_t to_alloc = 0;

	if (MP_OBJ_IS_SMALL_INT(input)) {
		int value = MP_OBJ_SMALL_INT_VALUE(input);
		to_alloc = cc_coder_sizeof_uint(value);
		if (alloc && cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		pos = *buffer;
		pos = cc_coder_encode_uint(pos, value);
	} else if (MP_OBJ_IS_INT(input)) {
		int32_t value = mp_obj_get_int(input);
		to_alloc = cc_coder_sizeof_int(value);
		if (alloc && cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		} else
		pos = *buffer;
		pos = cc_coder_encode_int(pos, value);
	}
#if CC_PYTHON_FLOATS
	else if (mp_obj_is_float(input)) {
		float value = mp_obj_float_get(input);
		to_alloc = cc_coder_sizeof_float(value);
		if (alloc && cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		pos = *buffer;
		pos = cc_coder_encode_float(pos, value);
	}
#endif
	else if (MP_OBJ_IS_TYPE(input, &mp_type_bool)) {
		bool value = false;
		if (input == mp_const_true)
			value = true;
		to_alloc = cc_coder_sizeof_bool(value);
		if (alloc && cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		pos = *buffer;
		pos = cc_coder_encode_bool(pos, value);
	} else if (MP_OBJ_IS_TYPE(input, &mp_type_NoneType)) {
		to_alloc = cc_coder_sizeof_nil();
		if (alloc && cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		pos = *buffer;
		pos = cc_coder_encode_nil(pos);
	} else if (MP_OBJ_IS_STR(input)) {
		const char *str = mp_obj_str_get_str(input);
		to_alloc = cc_coder_sizeof_str(strlen(str));
		if (alloc && cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		pos = *buffer;
		pos = cc_coder_encode_str(pos, str, strlen(str));
	} else if (MP_OBJ_IS_TYPE(input, &mp_type_tuple)) {
		cc_log_error("Tuple not implemented");
	} else if (MP_OBJ_IS_TYPE(input, &mp_type_list)) {
		mp_obj_list_t *list = MP_OBJ_TO_PTR(input);

		// TODO: Calculate buffer size
		if (alloc && cc_platform_mem_alloc((void **)buffer, 200) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}

		pos = *buffer;
		pos = cc_coder_encode_array(pos, list->len);
		for (size_t i = 0; i < list->len; i++) {
			if (cc_mpy_encode_from_mpy_obj(list->items[i], &pos, size, false) != CC_SUCCESS) {
				cc_log_error("Failed to encode Python object");
				if (alloc)
					cc_platform_mem_free(buffer);
				return CC_FAIL;
			}
			pos = pos + *size;
		}
	} else if (MP_OBJ_IS_TYPE(input, &mp_type_dict)) {
		mp_map_t *map = mp_obj_dict_get_map(input);
		uint kw_dict_len;
		kw_dict_len = mp_obj_dict_len(input);

		// TODO: Calculate buffer size
		if (alloc && cc_platform_mem_alloc((void **)buffer, 200) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}

		pos = *buffer;
		pos = cc_coder_encode_map(pos, kw_dict_len);
		for (size_t i = 0; i < kw_dict_len; i++) {
			if (cc_mpy_encode_from_mpy_obj(map->table[i].key, &pos, size, false) != CC_SUCCESS) {
				cc_log_error("Failed to encode Python object");
				if (alloc)
					cc_platform_mem_free(buffer);
				return CC_FAIL;
			}
			pos = pos + *size;
			if (cc_mpy_encode_from_mpy_obj(map->table[i].value, &pos, size, false) != CC_SUCCESS) {
				cc_log_error("Failed to encode Python object");
				if (alloc)
					cc_platform_mem_free(buffer);
				return CC_FAIL;
			}
			pos = pos + *size;
		}
	} else {
		cc_log_error("Unsupported type");
		return CC_FAIL;
	}

	*size = pos - *buffer;

	return CC_SUCCESS;
}
