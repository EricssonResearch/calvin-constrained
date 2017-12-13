/*
 * Copyright (c) 2016 Ericsson AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file mp_obj_get_int(num1)except in compliance with the License.
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
#include "py/runtime.h"
#include <string.h>
#include "py/objstr.h"
#include "calvinsys/cc_calvinsys.h"
#include "runtime/north/cc_actor.h"
#include "runtime/south/platform/cc_platform.h"
#include "cc_actor_mpy.h"
#include "runtime/north/coder/cc_coder.h"

static mp_obj_t cc_mp_obj_can_write(mp_obj_t arg_calvinsys, mp_obj_t arg_obj)
{
	bool can_write = false;
	cc_calvinsys_t *calvinsys = MP_OBJ_TO_PTR(arg_calvinsys);
	const char *obj_ref = mp_obj_str_get_str(arg_obj);

	can_write = cc_calvinsys_can_write(calvinsys, (char *)obj_ref);

	return mp_obj_new_bool(can_write);
}
static MP_DEFINE_CONST_FUN_OBJ_2(cc_mp_calvinsys_obj_can_write, cc_mp_obj_can_write);

static mp_obj_t cc_mp_obj_write(mp_obj_t arg_calvinsys, mp_obj_t arg_obj, mp_obj_t arg_data)
{
	bool result = false;
	char *data = NULL;
	size_t size = 0;
	cc_calvinsys_t *calvinsys = MP_OBJ_TO_PTR(arg_calvinsys);
	const char *obj_ref = mp_obj_str_get_str(arg_obj);

	if (cc_actor_mpy_encode_from_mpy_obj(arg_data, &data, &size) == CC_SUCCESS) {
		if (cc_calvinsys_write(calvinsys, (char *)obj_ref, data, size) == CC_SUCCESS)
			result = true;
		cc_platform_mem_free((void *)data);
	} else
		cc_log_error("Failed to encode object");

	return mp_obj_new_bool(result);
}
static MP_DEFINE_CONST_FUN_OBJ_3(cc_mp_calvinsys_obj_write, cc_mp_obj_write);

static mp_obj_t cc_mp_obj_can_read(mp_obj_t arg_calvinsys, mp_obj_t arg_obj)
{
	bool can_read = false;
	cc_calvinsys_t *calvinsys = MP_OBJ_TO_PTR(arg_calvinsys);
	const char *obj_ref = mp_obj_str_get_str(arg_obj);

	can_read = cc_calvinsys_can_read(calvinsys, (char *)obj_ref);

	return mp_obj_new_bool(can_read);
}
static MP_DEFINE_CONST_FUN_OBJ_2(cc_mp_calvinsys_obj_can_read, cc_mp_obj_can_read);

static mp_obj_t cc_mp_obj_read(mp_obj_t arg_calvinsys, mp_obj_t arg_obj)
{
	mp_obj_t value = MP_OBJ_NULL;
	char *data = NULL;
	size_t size = 0;
	cc_calvinsys_t *calvinsys = MP_OBJ_TO_PTR(arg_calvinsys);
	const char *obj_ref = mp_obj_str_get_str(arg_obj);

	if (cc_calvinsys_read(calvinsys, (char *)obj_ref, &data, &size) == CC_SUCCESS) {
		if (data == NULL)
			return mp_const_none;
		if (cc_actor_mpy_decode_to_mpy_obj(data, &value) == CC_SUCCESS) {
			cc_platform_mem_free((void *)data);
			return value;
		} else
			cc_log_error("Failed to decode data");
		cc_platform_mem_free((void *)data);
	} else
		cc_log_error("Failed to read calvinsys object");

	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(cc_mp_calvinsys_obj_read, cc_mp_obj_read);

static mp_obj_t cc_mp_calvinsys_open(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
	cc_actor_t *actor = MP_OBJ_TO_PTR(pos_args[0]);
	const char *name = mp_obj_str_get_str(pos_args[1]);
	char *obj_ref = NULL, *value = NULL;
	size_t i = 0, value_len = 0;
	cc_list_t *list = NULL, *item = NULL;
	cc_result_t result = CC_SUCCESS;

	for (i = 0; i < kw_args->alloc; i++) {
		if (MP_OBJ_IS_TYPE(kw_args->table[0].key, &mp_type_str)) {
			cc_log_error("Expected string as key");
			result = CC_FAIL;
			break;
		}

		const char *str = mp_obj_str_get_str(kw_args->table[0].key);

		if (cc_actor_mpy_encode_from_mpy_obj(kw_args->table[i].value, &value, &value_len) != CC_SUCCESS) {
			cc_log_error("Failed to decode '%s'", str);
			result = CC_FAIL;
			break;
		}

		if (cc_list_add_n(&list, str, strlen(str), value, value_len) == NULL) {
			cc_log_error("Failed to add '%s' as kwarg", str);
			result = CC_FAIL;
			break;
		}
	}

	if (result == CC_SUCCESS) {
		obj_ref = cc_calvinsys_open(actor, name, list);
		if (obj_ref == NULL) {
			cc_log_error("Failed to open '%s'", name);
			return mp_const_none;
		}
	}

	while (list != NULL) {
		item = list;
		list = list->next;
		cc_platform_mem_free(item->id);
		cc_platform_mem_free(item->data);
		cc_platform_mem_free(item);
	}

	if (result == CC_FAIL)
		return mp_const_none;

	return mp_obj_new_str(obj_ref, strlen(obj_ref), 0);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(cc_mp_calvinsys_open_obj, 2, cc_mp_calvinsys_open);

static mp_obj_t cc_mp_calvinsys_close(mp_obj_t arg_calvinsys, mp_obj_t arg_obj)
{
	cc_calvinsys_t *calvinsys = MP_OBJ_TO_PTR(arg_calvinsys);
	const char *obj_ref = mp_obj_str_get_str(arg_obj);

	cc_calvinsys_close(calvinsys, (char *)obj_ref);

	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(cc_mp_calvinsys_close_obj, cc_mp_calvinsys_close);

static const mp_map_elem_t cc_mp_calvinsys_globals_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_cc_mp_calvinsys)},
	{ MP_OBJ_NEW_QSTR(MP_QSTR_can_write), (mp_obj_t)&cc_mp_calvinsys_obj_can_write },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_write), (mp_obj_t)&cc_mp_calvinsys_obj_write },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_can_read), (mp_obj_t)&cc_mp_calvinsys_obj_can_read },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_read), (mp_obj_t)&cc_mp_calvinsys_obj_read },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_open), (mp_obj_t)&cc_mp_calvinsys_open_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_close), (mp_obj_t)&cc_mp_calvinsys_close_obj }
};

static MP_DEFINE_CONST_DICT(cc_mp_calvinsys_globals_dict, cc_mp_calvinsys_globals_table);

const mp_obj_module_t cc_mp_module_calvinsys = {
	.base = { &mp_type_module },
	.globals = (mp_obj_dict_t *)&cc_mp_calvinsys_globals_dict
};
