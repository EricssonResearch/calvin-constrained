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

#include <stdio.h>
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

cc_result_t cc_actor_mpy_encode_from_mpy_obj(mp_obj_t input, char **buffer, size_t *size)
{
	char *pos = NULL;
	size_t to_alloc = 0;

	if (MP_OBJ_IS_SMALL_INT(input)) {
		int value = MP_OBJ_SMALL_INT_VALUE(input);
		to_alloc = cc_coder_sizeof_uint(value);
		if (cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		pos = *buffer;
		pos = cc_coder_encode_uint(pos, value);
	} else if (MP_OBJ_IS_INT(input)) {
		int32_t value = mp_obj_get_int(input);
		to_alloc = cc_coder_sizeof_int(value);
		if (cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		} else
		pos = *buffer;
		pos = cc_coder_encode_int(pos, value);
	} else if (mp_obj_is_float(input)) {
		float value = mp_obj_float_get(input);
		to_alloc = cc_coder_sizeof_float(value);
		if (cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		pos = *buffer;
		pos = cc_coder_encode_float(pos, value);
	} else if (MP_OBJ_IS_TYPE(input, &mp_type_bool)) {
		bool value = false;
		if (input == mp_const_true)
			value = true;
		to_alloc = cc_coder_sizeof_bool(value);
		if (cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		pos = *buffer;
		pos = cc_coder_encode_bool(pos, value);
	} else if (MP_OBJ_IS_TYPE(input, &mp_type_NoneType)) {
		to_alloc = cc_coder_sizeof_nil();
		if (cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		pos = *buffer;
		pos = cc_coder_encode_nil(pos);
	} else if (MP_OBJ_IS_STR(input)) {
		const char *str = mp_obj_str_get_str(input);
		to_alloc = cc_coder_sizeof_str(strlen(str));
		if (cc_platform_mem_alloc((void **)buffer, to_alloc) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		pos = *buffer;
		pos = cc_coder_encode_str(pos, str, strlen(str));
	} else {
		cc_log_error("Unsupported type");
		return CC_FAIL;
	}

	*size = pos - *buffer;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_mpy_init(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	uint32_t j = 0, nbr_of_attributes = cc_list_count(managed_attributes);
	cc_list_t *list = managed_attributes;
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)(*actor)->instance_state;
	mp_obj_t args[2 + 2 * nbr_of_attributes];
	mp_obj_t tmp = MP_OBJ_NULL;
	cc_result_t result = CC_SUCCESS;
	int i = 0;
	mp_obj_t actor_init_method[2];

	mp_load_method(state->actor_class_instance, QSTR_FROM_STR_STATIC("init"), actor_init_method);

	args[0] = actor_init_method[0];
	args[1] = actor_init_method[1];
/*
	py_managed_list = mp_obj_new_list(nbr_of_attributes, NULL);
	mp_store_attr(state->actor_class_instance, QSTR_FROM_STR_STATIC("_managed"), py_managed_list);
*/
	while (list != NULL && result == CC_SUCCESS) {
		tmp = mp_obj_new_str(list->id, list->id_len, true);
		j += 2;
		args[j] = tmp;
		if (cc_actor_mpy_decode_to_mpy_obj(list->data, &tmp) != CC_SUCCESS) {
			cc_log_error("Failed to decode value from managed attribute");
			return CC_FAIL;
		}
		args[j + 1] = tmp;
		cc_log_debug("Added managed attribute '%s'", list->id);
		list = list->next;
	}

	// TODO catch exception
	mp_call_method_n_kw(0, nbr_of_attributes, args);

	for (i = 0; i < j; i++)
		args[i] = MP_OBJ_NULL;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_mpy_set_state(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	uint32_t nbr_of_attributes = cc_list_count(managed_attributes);
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)(*actor)->instance_state;
	uint32_t index = 0;
	cc_list_t *list = managed_attributes;
	mp_obj_t value = MP_OBJ_NULL;
	mp_obj_t py_managed_list = MP_OBJ_NULL;
	qstr q_attr;
	mp_obj_t attr = MP_OBJ_NULL;

	// Create the _managed list and the managed attributes on the Python instance
	py_managed_list = mp_obj_new_list(nbr_of_attributes, NULL);
	mp_store_attr(state->actor_class_instance, QSTR_FROM_STR_STATIC("_managed"), py_managed_list);
	while (list != NULL) {
		q_attr = qstr_from_strn(list->id, list->id_len);
		attr = mp_obj_new_str(list->id, list->id_len, true);
		if (cc_actor_mpy_decode_to_mpy_obj((char *)list->data, &value) != CC_SUCCESS) {
			cc_log_error("Failed to decode value from managed attribute");
			return CC_FAIL;
		}
		mp_obj_list_store(py_managed_list, MP_OBJ_NEW_SMALL_INT(index++), attr);
		mp_store_attr(state->actor_class_instance, q_attr, value);
		list = list->next;
	}

	value = MP_OBJ_NULL;
	py_managed_list = MP_OBJ_NULL;
	attr = MP_OBJ_NULL;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_mpy_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	qstr q_attr;
	mp_obj_t mpy_attr[2];
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
		if (!MP_OBJ_IS_STR(managed_list->items[i])) {
			cc_log_error("Attribute name is not a string");
			return CC_FAIL;
		}

		const char *attribute_name = mp_obj_str_get_str(managed_list->items[i]);

		q_attr = qstr_from_strn(attribute_name, strlen(attribute_name));

		mp_load_method(state->actor_class_instance, q_attr, mpy_attr);
		if (mpy_attr[0] == MP_OBJ_NULL) {
			cc_log_error("Failed to get managed attribute");
			return CC_FAIL;
		}

		if (cc_actor_mpy_encode_from_mpy_obj(mpy_attr[0], &packed_value, &size) != CC_SUCCESS) {
			cc_log_error("Failed to encode attribute");
			return CC_FAIL;
		}

		if (cc_list_add_n(managed_attributes, attribute_name, strlen(attribute_name), packed_value, size) == NULL) {
			cc_log_error("Failed to add attribute");
			cc_platform_mem_free(packed_value);
			return CC_FAIL;
		}
	}

	return CC_SUCCESS;
}

static bool cc_actor_mpy_fire(cc_actor_t *actor)
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

static void cc_actor_mpy_free_state(cc_actor_t *actor)
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

static void cc_actor_mpy_will_migrate(cc_actor_t *actor)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t mpy_will_migrate[2];

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("will_migrate"), mpy_will_migrate);
	if (mpy_will_migrate[0] != MP_OBJ_NULL && mpy_will_migrate[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, mpy_will_migrate);

	mpy_will_migrate[0] = MP_OBJ_NULL;
	mpy_will_migrate[1] = MP_OBJ_NULL;
}

static void cc_actor_mpy_will_end(cc_actor_t *actor)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t mpy_will_end[2];

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("will_end"), mpy_will_end);
	if (mpy_will_end[0] != MP_OBJ_NULL && mpy_will_end[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, mpy_will_end);

	mpy_will_end[0] = MP_OBJ_NULL;
	mpy_will_end[1] = MP_OBJ_NULL;
}

static void cc_actor_mpy_did_migrate(cc_actor_t *actor)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t mpy_did_migrate[2];

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("did_migrate"), mpy_did_migrate);
	if (mpy_did_migrate[0] != MP_OBJ_NULL && mpy_did_migrate[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, mpy_did_migrate);

	mpy_did_migrate[0] = MP_OBJ_NULL;
	mpy_did_migrate[1] = MP_OBJ_NULL;
}

char *cc_actor_mpy_get_path_from_type(char *type, uint32_t type_len, const char *extension, bool add_modules_dir)
{
	char *path = NULL;
	int i = 0, len = 0;

	if (add_modules_dir) {
	  len = type_len + strlen(CC_ACTOR_MODULES_DIR);
	  if (cc_platform_mem_alloc((void **)&path, len + strlen(extension) + 1) != CC_SUCCESS) {
	    cc_log_error("Failed to allocate memory");
	    return MP_IMPORT_STAT_NO_EXIST;
	  }
	  strncpy(path, CC_ACTOR_MODULES_DIR, strlen(CC_ACTOR_MODULES_DIR));
	  strncpy(path + strlen(CC_ACTOR_MODULES_DIR), type, type_len);
	  path[len] = '\0';
	} else {
		if (cc_platform_mem_alloc((void **)&path, type_len + strlen(extension) + 1) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return NULL;
		}

		strncpy(path, type, type_len);
		path[type_len] = '\0';
		len = type_len;
	}

	while (i < len) {
		if (path[i] == '.')
			path[i] = '/';
		i++;
	}

	for (i = 0; i < strlen(extension); i++)
		path[len + i] = extension[i];
	path[len + strlen(extension)] = '\0';

	return path;
}

cc_result_t cc_actor_mpy_init_from_type(cc_actor_t *actor)
{
	char *type = actor->type, *class = NULL, instance_name[30];
	qstr actor_type_qstr, actor_class_qstr;
	mp_obj_t args[2];
	cc_actor_mpy_state_t *state = NULL;
	mp_obj_t actor_module;
	mp_obj_t actor_class_ref[2];
	static int counter;
	uint8_t pos = strlen(actor->type), class_len = 0;

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
				break;
			}
			class_len++;
	}

	if (class == NULL) {
		cc_log_error("Expected '.' as class delimiter");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	actor_type_qstr = qstr_from_strn(type, strlen(actor->type));
	actor_class_qstr = qstr_from_strn(class, class_len);

	// load the module
	// you have to pass mp_const_true to the second argument in order to return reference
	// to the module instance, otherwise it will return a reference to the top-level package
	actor_module = mp_import_name(actor_type_qstr, mp_const_true, MP_OBJ_NEW_SMALL_INT(0));
	if (actor_module != MP_OBJ_NULL) {
		mp_store_global(qstr_from_strn(type, strlen(actor->type)), actor_module);
	} else {
		cc_log_error("No actor of type '%s'", actor->type);
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	// load the class
	mp_load_method(actor_module, actor_class_qstr, actor_class_ref);

	// pass the actor and calvinsys as a mp_obj_t
	args[0] = MP_OBJ_FROM_PTR(actor);
	args[1] = MP_OBJ_FROM_PTR(actor->calvinsys);
	state->actor_class_instance = mp_call_function_n_kw(actor_class_ref[0], 2, 0, args);
	mp_store_name(QSTR_FROM_STR_STATIC(instance_name), state->actor_class_instance);

	// load the fire method
	mp_load_method(state->actor_class_instance, QSTR_FROM_STR_STATIC("fire"), state->actor_fire_method);

	actor_module = MP_OBJ_NULL;
	actor_class_ref[0] = MP_OBJ_NULL;
	actor_class_ref[1] = MP_OBJ_NULL;

	actor->init = cc_actor_mpy_init;
	actor->fire = cc_actor_mpy_fire;
	actor->set_state = cc_actor_mpy_set_state;
	actor->get_managed_attributes = cc_actor_mpy_get_attributes;
	actor->free_state = cc_actor_mpy_free_state;
	actor->will_migrate = cc_actor_mpy_will_migrate;
	actor->will_end = cc_actor_mpy_will_end;
	actor->did_migrate = cc_actor_mpy_did_migrate;

	return CC_SUCCESS;
}

bool cc_actor_mpy_has_module(char *type)
{
	char *path = NULL;
	bool found = false;

	path = cc_actor_mpy_get_path_from_type(type, strlen(type), ".py", false);
	if (path != NULL) {
		found = mp_frozen_stat(path) == MP_IMPORT_STAT_FILE;
		cc_platform_mem_free(path);
		if (found)
			return true;
	}

	path = cc_actor_mpy_get_path_from_type(type, strlen(type), ".mpy", true);
	if (path != NULL) {
		found = cc_platform_file_stat(path) == CC_STAT_FILE;
		cc_platform_mem_free(path);
	}

	return found;
}

#endif
