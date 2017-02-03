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

#ifdef MICROPYTHON

#include <stddef.h>
#include <string.h>
#include "actor_mpy.h"
#include "../platform.h"
#include "../msgpack_helper.h"
#include "../msgpuck/msgpuck.h"
#include "py/frozenmod.h"
#include "py/gc.h"
#include "../libmpy/calvin_mpy_port.h"

result_t decode_to_mpy_obj(char *buffer, mp_obj_t *value)
{
	result_t result = FAIL;
	char *tmp_string = NULL;
	uint32_t len = 0, u_num = 0;
	int32_t num = 0;
	bool b = false;
	float f = 0;
	double d = 0;

	switch (mp_typeof(*buffer)) {
	case MP_NIL:
		*value = mp_const_none;
		return SUCCESS;
	case MP_STR:
		result = decode_str(buffer, &tmp_string, &len);
		if (result == SUCCESS) {
			*value = mp_obj_new_str(tmp_string, len, 0);
			return SUCCESS;
		}
		break;
	case MP_UINT:
		result = decode_uint(buffer, &u_num);
		if (result == SUCCESS) {
			*value = mp_obj_new_int_from_uint(u_num);
			return SUCCESS;
		}
		break;
	case MP_INT:
		result = decode_int(buffer, &num);
		if (result == SUCCESS) {
			*value = mp_obj_new_int(num);
			return SUCCESS;
		}
		break;
	case MP_BOOL:
		result = decode_bool(buffer, &b);
		if (result == SUCCESS) {
			*value = mp_obj_new_bool(b);
			return SUCCESS;
		}
		break;
	case MP_FLOAT:
		result = decode_float(buffer, &f);
		if (result == SUCCESS) {
			*value = mp_obj_new_float(f);
			return SUCCESS;
		}
		break;
	case MP_DOUBLE:
		result = decode_double(buffer, &d);
		if (result == SUCCESS) {
			*value = mp_obj_new_float(d);
			return SUCCESS;
		}
		break;
	default:
		log_error("Decoding not supported for this data '%d'", mp_typeof(*buffer));
	}

	return result;
}

result_t encode_from_mpy_obj(char **buffer, size_t *size, mp_obj_t input)
{
	uint32_t unum;
	int32_t num;
	double d;
	bool b;
	const char *str;
	char *tmp = NULL;

	if (MP_OBJ_IS_SMALL_INT(input)) {
		unum = MP_OBJ_SMALL_INT_VALUE(input);
		*size = mp_sizeof_uint(unum);
		if (platform_mem_alloc((void **)buffer, *size) == SUCCESS) {
			tmp = *buffer;
			mp_encode_uint(*buffer, unum);
			buffer = &tmp;
			return SUCCESS;
		}
		log_error("Failed to allocate '%ld' memory", (unsigned long)(*size));
		return FAIL;
	} else if (MP_OBJ_IS_INT(input)) {
		num = mp_obj_get_int(input);
		*size = mp_sizeof_int(num);
		if (platform_mem_alloc((void **)buffer, *size) == SUCCESS) {
			tmp = *buffer;
			mp_encode_int(*buffer, num);
			buffer = &tmp;
			return SUCCESS;
		}
		log_error("Failed to allocate '%ld' memory", (unsigned long)(*size));
		return FAIL;
	} else if (mp_obj_is_float(input)) {
		d = mp_obj_float_get(input);
		*size = mp_sizeof_double(d);
		if (platform_mem_alloc((void **)buffer, *size) == SUCCESS) {
			tmp = *buffer;
			mp_encode_double(*buffer, d);
			buffer = &tmp;
			return SUCCESS;
		}
		log_error("Failed to allocate '%ld' memory", (unsigned long)(*size));
		return FAIL;
	} else if (MP_OBJ_IS_TYPE(input, &mp_type_bool)) {
		b = mp_obj_new_bool(mp_obj_get_int(input));
		*size = mp_sizeof_bool(b);
		if (platform_mem_alloc((void **)buffer, *size) == SUCCESS) {
			tmp = *buffer;
			mp_encode_bool(*buffer, b);
			buffer = &tmp;
			return SUCCESS;
		}
		log_error("Failed to allocate '%ld' memory", (unsigned long)(*size));
		return FAIL;
	} else if (MP_OBJ_IS_TYPE(input, &mp_type_NoneType)) {
		*size = mp_sizeof_nil();
		*buffer = (char *)malloc(*size);
		if (platform_mem_alloc((void **)buffer, *size) == SUCCESS) {
			tmp = *buffer;
			mp_encode_nil(*buffer);
			buffer = &tmp;
			return SUCCESS;
		}
		log_error("Failed to allocate '%ld' memory", (unsigned long)(*size));
		return FAIL;
	} else if (MP_OBJ_IS_STR(input)) {
		str = mp_obj_get_type_str(input);
		*size = mp_sizeof_str(strlen(str));
		if (platform_mem_alloc((void **)buffer, *size) == SUCCESS) {
			tmp = *buffer;
			mp_encode_str(*buffer, str, strlen(str));
			buffer = &tmp;
			return SUCCESS;
		}
		log_error("Failed to allocate '%ld' memory", (unsigned long)(*size));
		return FAIL;
	}

	input = MP_OBJ_NULL;

	log_error("Encoding not supported for the data type");

	return FAIL;
}

result_t actor_mpy_init(actor_t **actor, list_t *attributes)
{
	uint32_t j = 0, nbr_of_attributes = list_count(attributes);
	list_t *list = attributes;
	ccmp_state_actor_t *state = (ccmp_state_actor_t *)(*actor)->instance_state;
	mp_obj_t args[2 + 2 * nbr_of_attributes];
	mp_obj_t tmp = MP_OBJ_NULL;
	result_t result = SUCCESS;
	int i = 0;
	mp_obj_t actor_init_method[2];

	mp_load_method(state->actor_class_instance, QSTR_FROM_STR_STATIC("init"), actor_init_method);

	args[0] = actor_init_method[0];
	args[1] = actor_init_method[1];

	while (list != NULL && result == SUCCESS) {
		tmp = mp_obj_new_str(list->id, strlen(list->id), true);
		j += 2;
		args[j] = tmp;
		result = decode_to_mpy_obj(list->data, &tmp);
		args[j + 1] = tmp;
		list = list->next;
	}

	// TODO catch exception
	mp_call_method_n_kw(0, nbr_of_attributes, args);

	for (i = 0; i < j; i++)
		args[i] = MP_OBJ_NULL;

	return SUCCESS;
}

result_t actor_mpy_set_state(actor_t **actor, list_t *attributes)
{
	uint32_t nbr_of_attributes = list_count(attributes);
	ccmp_state_actor_t *state = (ccmp_state_actor_t *)(*actor)->instance_state;
	result_t result = SUCCESS;
	uint32_t item = 0;
	list_t *list = attributes;
	mp_obj_t value = MP_OBJ_NULL;
	mp_obj_t py_managed_list = MP_OBJ_NULL;
	qstr q_attr;
	mp_obj_t attr = MP_OBJ_NULL;
	mp_obj_t mpy_did_migrate[2];

	// Copy to python world
	py_managed_list = mp_obj_new_list(nbr_of_attributes, NULL);
	mp_store_attr(state->actor_class_instance, QSTR_FROM_STR_STATIC("_managed"), py_managed_list);
	log_debug("in actor_mpy_set_state, after creating _managed list and associate it with actor instance");
	while (list != NULL && result == SUCCESS) {
		q_attr = QSTR_FROM_STR_STATIC(list->id);
		attr = mp_obj_new_str(list->id, strlen(list->id), true);
		result = decode_to_mpy_obj((char *)list->data, &value);
		if (result == SUCCESS) {
			mp_obj_list_store(py_managed_list, MP_OBJ_NEW_SMALL_INT(item), attr);
			mp_store_attr(state->actor_class_instance, q_attr, value);
			item++;
		}
		log_debug("Adding attribute %s", list->id);
		list = list->next;
	}

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("did_migrate"), mpy_did_migrate);
	if (mpy_did_migrate[0] != MP_OBJ_NULL && mpy_did_migrate[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, mpy_did_migrate);

	value = MP_OBJ_NULL;
	py_managed_list = MP_OBJ_NULL;
	attr = MP_OBJ_NULL;
	mpy_did_migrate[0] = MP_OBJ_NULL;
	mpy_did_migrate[1] = MP_OBJ_NULL;

	log_debug("Done storing managed attributes\n");

	return result;
}

result_t actor_mpy_get_managed_attributes(actor_t *actor, list_t **attributes)
{
	ccmp_state_actor_t *state = (ccmp_state_actor_t *)actor->instance_state;
	qstr q_attr;
	mp_obj_t mpy_attr[2], attr;
	mp_obj_list_t *managed_list = NULL;
	uint32_t i = 0;
	size_t size = 0;
	char *packed_value = NULL;
	mp_obj_t mpy_will_migrate[2];

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("will_migrate"), mpy_will_migrate);
	if (mpy_will_migrate[0] != MP_OBJ_NULL && mpy_will_migrate[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, mpy_will_migrate);

	mpy_will_migrate[0] = MP_OBJ_NULL;
	mpy_will_migrate[1] = MP_OBJ_NULL;

	mp_load_method(state->actor_class_instance, QSTR_FROM_STR_STATIC("_managed"), mpy_attr);
	if (mpy_attr[0] == MP_OBJ_NULL && mpy_attr[1] == MP_OBJ_NULL) {
		log_debug("No managed variables");
		return SUCCESS;
	}

	managed_list = MP_OBJ_TO_PTR(mpy_attr[0]);
	for (i = 0; i < managed_list->len; i++) {
		attr = managed_list->items[i];
		GET_STR_DATA_LEN(attr, name, len);
		q_attr = qstr_from_strn((const char *)name, len);
		mp_load_method(state->actor_class_instance, q_attr, mpy_attr);
		if (mpy_attr[0] == MP_OBJ_NULL) {
			log("Unknown managed attribute");
			continue;
		}

		if (encode_from_mpy_obj(&packed_value, &size, mpy_attr[0]) != SUCCESS)
			return FAIL;

		if (list_add_n(attributes, (char *)name, len, packed_value, size) != SUCCESS) {
			platform_mem_free(packed_value);
			return FAIL;
		}
	}

	return SUCCESS;
}

bool actor_mpy_fire(actor_t *actor)
{
	mp_obj_t res;
	ccmp_state_actor_t *state = (ccmp_state_actor_t *)actor->instance_state;
	bool did_fire = false;

	res = mp_call_method_n_kw(0, 0, state->actor_fire_method);
	if (res != mp_const_none && mp_obj_is_true(res))
		did_fire = true;
	res = MP_OBJ_NULL;

	gc_collect();
#ifdef DEBUG_MEM
	gc_dump_info();
#endif

	return did_fire;
}

void actor_mpy_free_state(actor_t *actor)
{
	ccmp_state_actor_t *state = (ccmp_state_actor_t *)actor->instance_state;
	mp_obj_t mpy_will_end[2];

	if (state != NULL) {
		mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("will_end"), mpy_will_end);
		if (mpy_will_end[0] != MP_OBJ_NULL && mpy_will_end[1] != MP_OBJ_NULL)
			mp_call_method_n_kw(0, 0, mpy_will_end);

		// TODO: gc_collect does not free objects, fix it.
		state->actor_class_instance = MP_OBJ_NULL;

		platform_mem_free((void *)state);
		actor->instance_state = NULL;
	}

	gc_collect();
#ifdef DEBUG_MEM
	gc_dump_info();
#endif
}

result_t actor_mpy_init_from_type(actor_t *actor, char *type, uint32_t type_len)
{
	char *tmp = NULL, instance_name[30];
	qstr actor_type_qstr, actor_class_qstr;
	mp_obj_t args[1];
	ccmp_state_actor_t *state = NULL;
	mp_obj_t actor_module;
	mp_obj_t actor_class_ref[2];
	static int counter;

	sprintf(instance_name, "actor_obj%d", counter++);

	if (platform_mem_alloc((void **)&state, sizeof(ccmp_state_actor_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}
	memset(state, 0, sizeof(ccmp_state_actor_t));
	actor->instance_state = (void *)state;

	// get the class name (the part after the last "." in the passed module type string
	tmp = type;
	while (strstr(tmp, ".") != NULL)
		tmp = strstr(tmp, ".") + 1;
	actor_type_qstr = QSTR_FROM_STR_STATIC(actor->type);
	actor_class_qstr = QSTR_FROM_STR_STATIC(tmp);

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

	actor->init = actor_mpy_init;
	actor->fire = actor_mpy_fire;
	actor->set_state = actor_mpy_set_state;
	actor->get_managed_attributes = actor_mpy_get_managed_attributes;
	actor->free_state = actor_mpy_free_state;

	return SUCCESS;
}

#endif
