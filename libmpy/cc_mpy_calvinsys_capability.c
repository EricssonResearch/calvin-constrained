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
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "runtime/north/cc_actor.h"
#include "runtime/south/platform/cc_platform.h"
#include "runtime/north/coder/cc_coder.h"
#include "py/frozenmod.h"
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/lexer.h"
#include "cc_mpy_common.h"

typedef struct cc_mpy_calvinsys_state_t {
	mp_obj_t class_instance;
} cc_mpy_calvinsys_state_t;

static bool cc_mpy_calvinsys_can_write(cc_calvinsys_obj_t *obj)
{
	mp_obj_t res;
	cc_mpy_calvinsys_state_t *state = (cc_mpy_calvinsys_state_t *)obj->state;
	bool can_write = false;
	mp_obj_t func[2];

	mp_load_method_maybe(state->class_instance, QSTR_FROM_STR_STATIC("can_write"), func);
	if (func[0] != MP_OBJ_NULL && func[1] != MP_OBJ_NULL) {
		res = mp_call_method_n_kw(0, 0, func);
		if (res != mp_const_none && mp_obj_is_true(res))
			can_write = true;
	}

	func[0] = MP_OBJ_NULL;
	func[1] = MP_OBJ_NULL;
	res = MP_OBJ_NULL;

	return can_write;
}

static cc_result_t cc_mpy_calvinsys_write(cc_calvinsys_obj_t *obj, char *data, size_t data_size)
{
	mp_obj_t res;
	cc_mpy_calvinsys_state_t *state = (cc_mpy_calvinsys_state_t *)obj->state;
	mp_obj_t func[4];
	cc_result_t result = CC_FAIL;
	mp_obj_t tmp = MP_OBJ_NULL;

	mp_load_method_maybe(state->class_instance, QSTR_FROM_STR_STATIC("write"), func);
	if (func[0] != MP_OBJ_NULL && func[1] != MP_OBJ_NULL) {
		tmp = mp_obj_new_str("data", 4);
		func[2] = tmp;
		if (cc_mpy_decode_to_mpy_obj(data, &tmp) != CC_SUCCESS)
			cc_log_error("Failed to create Python arg");
		else {
			func[3] = tmp;
			res = mp_call_method_n_kw(0, 1, func);
			if (res != mp_const_none && mp_obj_is_true(res))
				result = CC_SUCCESS;
		}
	}

	func[0] = MP_OBJ_NULL;
	func[1] = MP_OBJ_NULL;
	func[2] = MP_OBJ_NULL;
	func[3] = MP_OBJ_NULL;
	res = MP_OBJ_NULL;

	return result;
}

static bool cc_mpy_calvinsys_can_read(cc_calvinsys_obj_t *obj)
{
	mp_obj_t res;
	cc_mpy_calvinsys_state_t *state = (cc_mpy_calvinsys_state_t *)obj->state;
	bool can_read = false;
	mp_obj_t func[2];

	mp_load_method_maybe(state->class_instance, QSTR_FROM_STR_STATIC("can_read"), func);
	if (func[0] != MP_OBJ_NULL && func[1] != MP_OBJ_NULL) {
		res = mp_call_method_n_kw(0, 0, func);
		if (res != mp_const_none && mp_obj_is_true(res))
			can_read = true;
	}

	func[0] = MP_OBJ_NULL;
	func[1] = MP_OBJ_NULL;
	res = MP_OBJ_NULL;

	return can_read;
}

static cc_result_t cc_mpy_calvinsys_read(cc_calvinsys_obj_t *obj, char **data, size_t *data_size)
{
	cc_result_t result = CC_SUCCESS;
	mp_obj_t res;
	cc_mpy_calvinsys_state_t *state = (cc_mpy_calvinsys_state_t *)obj->state;
	mp_obj_t func[2];

	mp_load_method_maybe(state->class_instance, QSTR_FROM_STR_STATIC("read"), func);
	if (func[0] != MP_OBJ_NULL && func[1] != MP_OBJ_NULL) {
		res = mp_call_method_n_kw(0, 0, func);
		if (res == mp_const_none) {
			*data = NULL;
			*data_size = 0;
			result = CC_FAIL;
		} else {
			if (cc_mpy_encode_from_mpy_obj(res, data, data_size, true) != CC_SUCCESS) {
				cc_log_error("Failed to encode Python object");
				result = CC_FAIL;
			}
		}
	}

	func[0] = MP_OBJ_NULL;
	func[1] = MP_OBJ_NULL;
	res = MP_OBJ_NULL;

	return result;
}

static cc_result_t cc_mpy_calvinsys_close(cc_calvinsys_obj_t *obj)
{
	cc_mpy_calvinsys_state_t *state = (cc_mpy_calvinsys_state_t *)obj->state;
	mp_obj_t func[2];

	mp_load_method_maybe(state->class_instance, QSTR_FROM_STR_STATIC("close"), func);
	if (func[0] != MP_OBJ_NULL && func[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, func);

	cc_platform_mem_free((void *)obj->state);

	func[0] = MP_OBJ_NULL;
	func[1] = MP_OBJ_NULL;

	return CC_SUCCESS;
}

static char *cc_mpy_calvinsys_serialize(char *id, cc_calvinsys_obj_t *obj, char *buffer)
{
	mp_obj_t res;
	cc_mpy_calvinsys_state_t *state = (cc_mpy_calvinsys_state_t *)obj->state;
	mp_obj_t func[2];
	size_t size = 0;
	char *w = buffer;

	mp_load_method_maybe(state->class_instance, QSTR_FROM_STR_STATIC("serialize"), func);
	if (func[0] != MP_OBJ_NULL && func[1] != MP_OBJ_NULL) {
		res = mp_call_method_n_kw(0, 0, func);
		if (res != mp_const_none) {
			if (MP_OBJ_IS_TYPE(res, &mp_type_dict)) {
				mp_map_t *map = mp_obj_dict_get_map(res);
				uint kw_dict_len;
				kw_dict_len = mp_obj_dict_len(res);

			 	w = cc_coder_encode_kv_map(w, "obj", kw_dict_len);
			 	w = w + size;
				for (size_t i = 0; i < kw_dict_len; i++) {
					if (cc_mpy_encode_from_mpy_obj(map->table[i].key, &w, &size, false) != CC_SUCCESS) {
						cc_log_error("Failed to encode Python object");
						w = NULL;
						break;
					}
					w = w + size;
					if (cc_mpy_encode_from_mpy_obj(map->table[i].value, &w, &size, false) != CC_SUCCESS) {
						cc_log_error("Failed to encode Python object");
						w = NULL;
						break;
					}
					w = w + size;
				}
			 } else
				cc_log_error("Serialized value is NOT dict");
		} else
			cc_log_error("Serialize returned NULL");
	} else
		cc_log_error("Failed to load serialize method");

	func[0] = MP_OBJ_NULL;
	func[1] = MP_OBJ_NULL;
	res = MP_OBJ_NULL;

	return w;
}

static cc_result_t cc_mpy_calvinsys_object_load(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	char *type = NULL, *class = NULL, instance_name[30];
	qstr type_qstr, class_qstr;
	cc_mpy_calvinsys_state_t *state = NULL;
	mp_obj_t module;
	mp_obj_t class_ref[2];
	static int counter;
	uint8_t pos = 0, class_len = 0;
	mp_obj_t args[3];

	cc_log("Loading Python capability '%s' from '%s'", obj->capability->name, obj->capability->python_module);

	sprintf(instance_name, "capability_obj%d", counter++);

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_mpy_calvinsys_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	memset(state, 0, sizeof(cc_mpy_calvinsys_state_t));
	obj->state = (void *)state;

	// + 10 for calvinsys/
	if (cc_platform_mem_alloc((void **)&type, strlen(obj->capability->python_module) + 10 + 1) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	memset(type, 0, 10 + strlen(obj->capability->python_module) + 1);
	strncpy(type, "calvinsys.", 10);
	strncpy(type + strlen(type), obj->capability->python_module, strlen(obj->capability->python_module));
	pos = strlen(type);

	// get the class name (the part after the last "." in the passed module type string
	while (pos-- > 0) {
			if (type[pos] == '.') {
				class = type + (pos + 1);
				break;
			}
			class_len++;
	}

	if (class == NULL) {
		cc_log_error("Failed to get python class name from '%s'", type);
		cc_platform_mem_free(state);
		cc_platform_mem_free(type);
		return CC_FAIL;
	}

	type_qstr = qstr_from_strn(type, strlen(type));
	class_qstr = qstr_from_strn(class, class_len);

	// load the module
	// you have to pass mp_const_true to the second argument in order to return reference
	// to the module instance, otherwise it will return a reference to the top-level package
	module = mp_import_name(type_qstr, mp_const_true, MP_OBJ_NEW_SMALL_INT(0));
	if (module != MP_OBJ_NULL) {
		mp_store_global(qstr_from_strn(type, strlen(type)), module);
	} else {
		cc_log_error("Failed to import '%s'", type);
		cc_platform_mem_free(state);
		cc_platform_mem_free(type);
		return CC_FAIL;
	}

	// load the class
	mp_load_method(module, class_qstr, class_ref);

	// pass the actor and calvinsys as a mp_obj_t
	args[0] = MP_OBJ_FROM_PTR(obj->capability->calvinsys);
	args[1] = MP_OBJ_FROM_PTR(obj->capability->name);
	args[2] = MP_OBJ_FROM_PTR(obj->actor);
	state->class_instance = mp_call_function_n_kw(class_ref[0], 3, 0, args);
	mp_store_name(QSTR_FROM_STR_STATIC(instance_name), state->class_instance);

	module = MP_OBJ_NULL;
	class_ref[0] = MP_OBJ_NULL;
	class_ref[1] = MP_OBJ_NULL;

	obj->can_write = cc_mpy_calvinsys_can_write;
	obj->write = cc_mpy_calvinsys_write;
	obj->can_read = cc_mpy_calvinsys_can_read;
	obj->read = cc_mpy_calvinsys_read;
	obj->can_write = cc_mpy_calvinsys_can_write;
	obj->close = cc_mpy_calvinsys_close;
	obj->serialize = cc_mpy_calvinsys_serialize;

	cc_platform_mem_free(type);

	return CC_SUCCESS;
}


cc_result_t cc_mpy_calvinsys_object_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	cc_mpy_calvinsys_state_t *state = NULL;
	uint32_t j = 0, nbr_of_attributes = cc_list_count(kwargs);
	cc_result_t result = CC_SUCCESS;
	cc_list_t *list = kwargs;
	mp_obj_t func[2 + 2 * nbr_of_attributes];
	mp_obj_t tmp = MP_OBJ_NULL;
	int i = 0;

	if ((result = cc_mpy_calvinsys_object_load(obj, kwargs)) != CC_SUCCESS)
		cc_log_error("Failed to load Python capability '%s' from '%s'", obj->capability->name, obj->capability->python_module);

	state = (cc_mpy_calvinsys_state_t *)obj->state;

	if (result == CC_SUCCESS) {
		mp_load_method(state->class_instance, QSTR_FROM_STR_STATIC("init"), func);
		if (func[0] == MP_OBJ_NULL || func[1] == MP_OBJ_NULL) {
			cc_log_error("Failed to call init");
			result = CC_FAIL;
		}

		while (list != NULL && result == CC_SUCCESS) {
			tmp = mp_obj_new_str(list->id, list->id_len);
			j += 2;
			func[j] = tmp;
			if (cc_mpy_decode_to_mpy_obj(list->data, &tmp) != CC_SUCCESS) {
				cc_log_error("Failed to decode value from arg");
				result = CC_FAIL;
				break;
			}
			func[j + 1] = tmp;
			list = list->next;
		}

		if (result == CC_SUCCESS) {
			mp_call_method_n_kw(0, nbr_of_attributes, func);
		}
	}

	for (i = 0; i < j; i++)
		func[i] = MP_OBJ_NULL;

	return result;
}

cc_result_t cc_mpy_calvinsys_object_deserialize(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	cc_result_t result = CC_SUCCESS;
	cc_mpy_calvinsys_state_t *state = NULL;
	cc_list_t *list = kwargs;
	mp_obj_t value = MP_OBJ_NULL;
	qstr q_attr;

	if ((result = cc_mpy_calvinsys_object_load(obj, kwargs)) != CC_SUCCESS)
		cc_log_error("Failed to load Python capability '%s' from '%s'", obj->capability->name, obj->capability->python_module);

	state = (cc_mpy_calvinsys_state_t *)obj->state;

	while (list != NULL && result == CC_SUCCESS) {
		q_attr = qstr_from_strn(list->id, list->id_len);
		if (cc_mpy_decode_to_mpy_obj((char *)list->data, &value) != CC_SUCCESS) {
			cc_log_error("Failed to decode value from managed attribute");
			result = CC_SUCCESS;
			break;
		}
		mp_store_attr(state->class_instance, q_attr, value);
		list = list->next;
	}

	value = MP_OBJ_NULL;

	return result;
}
