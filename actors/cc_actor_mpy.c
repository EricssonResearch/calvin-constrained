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

#ifdef CC_PYTHON_ENABLED

#include <stddef.h>
#include <string.h>
#include "cc_actor_mpy.h"
#include "../runtime/south/platform/cc_platform.h"
#include "../runtime/north/coder/cc_coder.h"
#include "py/frozenmod.h"
#include "py/gc.h"
#include "../libmpy/cc_mpy_port.h"

cc_result_t cc_actor_mpy_decode_to_mpy_obj(char *buffer, mp_obj_t *value)
{
	cc_result_t result = CC_FAIL;
	char *tmp_string = NULL;
	uint32_t len = 0, u_num = 0;
	int32_t num = 0;
	bool b = false;
	float f = 0;
	double d = 0;

	switch (cc_coder_type_of(buffer)) {
	case CC_CODER_NIL:
		*value = mp_const_none;
		return CC_SUCCESS;
	case CC_CODER_STR:
		result = cc_coder_decode_str(buffer, &tmp_string, &len);
		if (result == CC_SUCCESS) {
			*value = mp_obj_new_str(tmp_string, len, 0);
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
			*value = mp_obj_new_float(f);
			return CC_SUCCESS;
		}
		break;
	case CC_CODER_DOUBLE:
		result = cc_coder_decode_double(buffer, &d);
		if (result == CC_SUCCESS) {
			*value = mp_obj_new_float(d);
			return CC_SUCCESS;
		}
		break;
	default:
		cc_log_error("Unsupported type");
	}

	return result;
}

static bool cc_actor_mpy_can_encode(uint32_t size, uint32_t buffer_size, char *start, char *pos)
{
	if (((pos + size) - start) <= buffer_size)
		return true;

	return false;
}

cc_result_t cc_actor_mpy_encode_mpy_map(mp_map_t *map, char **buffer, size_t buffer_size)
{
	uint i = 0;
	char *pos = *buffer;

	if (map->alloc > 0) {
		pos = cc_coder_encode_map(pos, map->alloc);
		for (i = 0; i < map->alloc; i++) {
			const char *key = mp_obj_str_get_str(map->table[i].key);
			if (!cc_actor_mpy_can_encode(cc_coder_sizeof_str(strlen(key)), buffer_size, *buffer, pos)) {
				cc_log_error("Buffer to small");
				return CC_FAIL;
			}
			pos = cc_coder_encode_str(pos, key, strlen(key));

			if (MP_OBJ_IS_SMALL_INT(map->table[i].value)) {
				int value = MP_OBJ_SMALL_INT_VALUE(map->table[i].value);
				if (!cc_actor_mpy_can_encode(cc_coder_sizeof_uint(value), buffer_size, *buffer, pos)) {
					cc_log_error("Buffer to small");
					return CC_FAIL;
				}
				pos = cc_coder_encode_uint(pos, value);
			} else if (MP_OBJ_IS_INT(map->table[i].value)) {
				int32_t value = mp_obj_get_int(map->table[i].value);
				if (!cc_actor_mpy_can_encode(cc_coder_sizeof_int(value), buffer_size, *buffer, pos)) {
					cc_log_error("Buffer to small");
					return CC_FAIL;
				}
				pos = cc_coder_encode_int(pos, mp_obj_get_int(map->table[i].value));
			} else if (mp_obj_is_float(map->table[i].value)) {
				float value = mp_obj_float_get(map->table[i].value);
				if (!cc_actor_mpy_can_encode(cc_coder_sizeof_float(value), buffer_size, *buffer, pos)) {
					cc_log_error("Buffer to small");
					return CC_FAIL;
				}
				pos = cc_coder_encode_double(pos, value);
			} else if (MP_OBJ_IS_TYPE(map->table[i].value, &mp_type_bool)) {
				bool value = mp_obj_new_bool(mp_obj_get_int(map->table[i].value));
				if (!cc_actor_mpy_can_encode(cc_coder_sizeof_bool(value), buffer_size, *buffer, pos)) {
					cc_log_error("Buffer to small");
					return CC_FAIL;
				}
				pos = cc_coder_encode_bool(pos, value);
			} else if (MP_OBJ_IS_TYPE(map->table[i].value, &mp_type_NoneType)) {
				if (!cc_actor_mpy_can_encode(cc_coder_sizeof_nil(), buffer_size, *buffer, pos)) {
					cc_log_error("Buffer to small");
					return CC_FAIL;
				}
				pos = cc_coder_encode_nil(pos);
			} else if (MP_OBJ_IS_STR(map->table[i].value)) {
				mp_uint_t len;
				const char *s = mp_obj_str_get_data(map->table[i].value, &len);
				if (!cc_actor_mpy_can_encode(cc_coder_sizeof_str(strlen(s)), buffer_size, *buffer, pos)) {
					cc_log_error("Buffer to small");
					return CC_FAIL;
				}
				pos = cc_coder_encode_str(pos, s, strlen(s));
			} else {
				cc_log_error("Unsupported type");
				return CC_FAIL;
			}
		}
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_mpy_encode_from_mpy_obj(mp_obj_t input, char **buffer, size_t *size)
{
	char *pos = *buffer;
	cc_result_t result = CC_SUCCESS;

	if (MP_OBJ_IS_SMALL_INT(input)) {
		int value = MP_OBJ_SMALL_INT_VALUE(input);
		*size = cc_coder_sizeof_uint(value);
		if (cc_platform_mem_alloc((void **)buffer, *size) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			result = CC_FAIL;
		} else
			pos = cc_coder_encode_uint(*buffer, value);
	} else if (MP_OBJ_IS_INT(input)) {
		int32_t value = mp_obj_get_int(input);
		*size = cc_coder_sizeof_int(value);
		if (cc_platform_mem_alloc((void **)buffer, *size) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			result = CC_FAIL;
		} else
			pos = cc_coder_encode_int(*buffer, value);
	} else if (mp_obj_is_float(input)) {
		float value = mp_obj_float_get(input);
		*size = cc_coder_sizeof_float(value);
		if (cc_platform_mem_alloc((void **)buffer, *size) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			result = CC_FAIL;
		} else
			pos = cc_coder_encode_float(*buffer, value);
	} else if (MP_OBJ_IS_TYPE(input, &mp_type_bool)) {
		bool value = mp_obj_new_bool(mp_obj_get_int(input));
		*size = cc_coder_sizeof_bool(value);
		if (cc_platform_mem_alloc((void **)buffer, *size) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			result = CC_FAIL;
		} else
			pos = cc_coder_encode_bool(*buffer, value);
	} else if (MP_OBJ_IS_TYPE(input, &mp_type_NoneType)) {
		*size = cc_coder_sizeof_nil();
		if (cc_platform_mem_alloc((void **)buffer, *size) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			result = CC_FAIL;
		}
		pos = cc_coder_encode_nil(*buffer);
	} else if (MP_OBJ_IS_STR(input)) {
		const char *str = mp_obj_str_get_str(input);
		*size = cc_coder_sizeof_str(strlen(str));
		if (cc_platform_mem_alloc((void **)buffer, *size) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			result = CC_FAIL;
		}
		pos = cc_coder_encode_str(*buffer, str, strlen(str));
	} else {
		cc_log_error("Unsupported type");
		result = CC_FAIL;
	}

	if ((pos - *buffer) > *size)
		cc_log_error("OPS, buffer is to small");

	return result;
}

static cc_result_t cc_actor_mpy_init(cc_actor_t**actor, cc_list_t *attributes)
{
	uint32_t j = 0, nbr_of_attributes = cc_list_count(attributes);
	cc_list_t *list = attributes;
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)(*actor)->instance_state;
	mp_obj_t args[2 + 2 * nbr_of_attributes];
	mp_obj_t tmp = MP_OBJ_NULL;
	cc_result_t result = CC_SUCCESS;
	int i = 0;
	mp_obj_t actor_init_method[2];

	mp_load_method(state->actor_class_instance, QSTR_FROM_STR_STATIC("init"), actor_init_method);

	args[0] = actor_init_method[0];
	args[1] = actor_init_method[1];

	while (list != NULL && result == CC_SUCCESS) {
		tmp = mp_obj_new_str(list->id, strlen(list->id), true);
		j += 2;
		args[j] = tmp;
		result = cc_actor_mpy_decode_to_mpy_obj(list->data, &tmp);
		args[j + 1] = tmp;
		list = list->next;
	}

	// TODO catch exception
	mp_call_method_n_kw(0, nbr_of_attributes, args);

	for (i = 0; i < j; i++)
		args[i] = MP_OBJ_NULL;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_mpy_set_state(cc_actor_t**actor, cc_list_t *attributes)
{
	uint32_t nbr_of_attributes = cc_list_count(attributes);
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)(*actor)->instance_state;
	cc_result_t result = CC_SUCCESS;
	uint32_t item = 0;
	cc_list_t *list = attributes;
	mp_obj_t value = MP_OBJ_NULL;
	mp_obj_t py_managed_list = MP_OBJ_NULL;
	qstr q_attr;
	mp_obj_t attr = MP_OBJ_NULL;

	// Copy to python world
	py_managed_list = mp_obj_new_list(nbr_of_attributes, NULL);
	mp_store_attr(state->actor_class_instance, QSTR_FROM_STR_STATIC("_managed"), py_managed_list);
	cc_log_debug("in actor_mpy_set_state, after creating _managed list and associate it with actor instance");
	while (list != NULL && result == CC_SUCCESS) {
		q_attr = QSTR_FROM_STR_STATIC(list->id);
		attr = mp_obj_new_str(list->id, strlen(list->id), true);
		result = cc_actor_mpy_decode_to_mpy_obj((char *)list->data, &value);
		if (result == CC_SUCCESS) {
			mp_obj_list_store(py_managed_list, MP_OBJ_NEW_SMALL_INT(item), attr);
			mp_store_attr(state->actor_class_instance, q_attr, value);
			item++;
		}
		cc_log_debug("Adding attribute %s", list->id);
		list = list->next;
	}

	value = MP_OBJ_NULL;
	py_managed_list = MP_OBJ_NULL;
	attr = MP_OBJ_NULL;

	cc_log_debug("Done storing managed attributes\n");

	return result;
}

static cc_result_t cc_actor_mpy_get_managed_attributes(cc_actor_t*actor, cc_list_t **attributes)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	qstr q_attr;
	mp_obj_t mpy_attr[2], attr;
	mp_obj_list_t *managed_list = NULL;
	uint32_t i = 0;
	size_t size = 0;
	char *packed_value = NULL;

	mp_load_method(state->actor_class_instance, QSTR_FROM_STR_STATIC("_managed"), mpy_attr);
	if (mpy_attr[0] == MP_OBJ_NULL && mpy_attr[1] == MP_OBJ_NULL) {
		cc_log_debug("No managed variables");
		return CC_SUCCESS;
	}

	managed_list = MP_OBJ_TO_PTR(mpy_attr[0]);
	for (i = 0; i < managed_list->len; i++) {
		attr = managed_list->items[i];
		GET_STR_DATA_LEN(attr, name, len);
		q_attr = qstr_from_strn((const char *)name, len);
		mp_load_method(state->actor_class_instance, q_attr, mpy_attr);
		if (mpy_attr[0] == MP_OBJ_NULL) {
			cc_log_debug("Unknown managed attribute");
			continue;
		}

		if (cc_actor_mpy_encode_from_mpy_obj(mpy_attr[0], &packed_value, &size) != CC_SUCCESS)
			return CC_FAIL;

		if (cc_list_add_n(attributes, (char *)name, len, packed_value, size) != CC_SUCCESS) {
			cc_platform_mem_free(packed_value);
			return CC_FAIL;
		}
	}

	return CC_SUCCESS;
}

static bool cc_actor_mpy_fire(cc_actor_t*actor)
{
	mp_obj_t res;
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	bool did_fire = false;

	res = mp_call_method_n_kw(0, 0, state->actor_fire_method);
	if (res != mp_const_none && mp_obj_is_true(res))
		did_fire = true;
	res = MP_OBJ_NULL;

	gc_collect();
#ifdef CC_DEBUG_MEM
	gc_dump_info();
#endif

	return did_fire;
}

static void cc_actor_mpy_free_state(cc_actor_t*actor)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;

	if (state != NULL) {
		// TODO: gc_collect does not free objects, fix it.
		state->actor_class_instance = MP_OBJ_NULL;
		cc_platform_mem_free((void *)state);
		actor->instance_state = NULL;
	}

	gc_collect();
#ifdef CC_DEBUG_MEM
	gc_dump_info();
#endif
}

static void cc_actor_mpy_will_migrate(cc_actor_t*actor)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t mpy_will_migrate[2];

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("will_migrate"), mpy_will_migrate);
	if (mpy_will_migrate[0] != MP_OBJ_NULL && mpy_will_migrate[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, mpy_will_migrate);

	mpy_will_migrate[0] = MP_OBJ_NULL;
	mpy_will_migrate[1] = MP_OBJ_NULL;
}

static void cc_actor_mpy_will_end(cc_actor_t*actor)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t mpy_will_end[2];

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("will_end"), mpy_will_end);
	if (mpy_will_end[0] != MP_OBJ_NULL && mpy_will_end[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, mpy_will_end);

	mpy_will_end[0] = MP_OBJ_NULL;
	mpy_will_end[1] = MP_OBJ_NULL;
}

static void cc_actor_mpy_did_migrate(cc_actor_t*actor)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t mpy_did_migrate[2];

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("did_migrate"), mpy_did_migrate);
	if (mpy_did_migrate[0] != MP_OBJ_NULL && mpy_did_migrate[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, mpy_did_migrate);

	mpy_did_migrate[0] = MP_OBJ_NULL;
	mpy_did_migrate[1] = MP_OBJ_NULL;
}

cc_result_t cc_actor_mpy_init_from_type(cc_actor_t*actor, char *type, uint32_t type_len)
{
	char *class = NULL, instance_name[30];
	qstr actor_type_qstr, actor_class_qstr;
	mp_obj_t args[1];
	cc_actor_mpy_state_t *state = NULL;
	mp_obj_t actor_module;
	mp_obj_t actor_class_ref[2];
	static int counter;
	uint8_t pos = type_len;

	sprintf(instance_name, "actor_obj%d", counter++);

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_mpy_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	memset(state, 0, sizeof(cc_actor_mpy_state_t));
	actor->instance_state = (void *)state;

	// get the class name (the part after the last "." in the passed module type string
	while (pos-- > 0) {
			if (type[pos] == '.') {
				class = type + (pos + 1);
			}
	}

	if (class == NULL) {
		cc_log_error("Expected '.' as class delimiter");
		return CC_FAIL;
	}

	actor_type_qstr = QSTR_FROM_STR_STATIC(actor->type);
	actor_class_qstr = QSTR_FROM_STR_STATIC(class);

	// load the module
	// you have to pass mp_const_true to the second argument in order to return reference
	// to the module instance, otherwise it will return a reference to the top-level package
	actor_module = mp_import_name(actor_type_qstr, mp_const_true, MP_OBJ_NEW_SMALL_INT(0));
	mp_store_global(actor_type_qstr, actor_module);

	// load the class
	mp_load_method(actor_module, actor_class_qstr, actor_class_ref);

	//pass the actor as a mp_obj_t
	args[0] = MP_OBJ_FROM_PTR(actor);
	state->actor_class_instance = mp_call_function_n_kw(actor_class_ref[0], 1, 0, args);
	mp_store_name(QSTR_FROM_STR_STATIC(instance_name), state->actor_class_instance);

	// load the fire method
	mp_load_method(state->actor_class_instance, QSTR_FROM_STR_STATIC("fire"), state->actor_fire_method);

	actor_module = MP_OBJ_NULL;
	actor_class_ref[0] = MP_OBJ_NULL;
	actor_class_ref[1] = MP_OBJ_NULL;

	actor->init = cc_actor_mpy_init;
	actor->fire = cc_actor_mpy_fire;
	actor->set_state = cc_actor_mpy_set_state;
	actor->get_managed_attributes = cc_actor_mpy_get_managed_attributes;
	actor->free_state = cc_actor_mpy_free_state;
	actor->will_migrate = cc_actor_mpy_will_migrate;
	actor->will_end = cc_actor_mpy_will_end;
	actor->did_migrate = cc_actor_mpy_did_migrate;

	return CC_SUCCESS;
}

#endif
