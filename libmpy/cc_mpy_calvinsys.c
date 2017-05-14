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
#include "../calvinsys/cc_calvinsys.h"
#include "../runtime/north/cc_actor.h"
#include "../runtime/south/platform/cc_platform.h"
#include "../actors/cc_actor_mpy.h"

typedef struct cc_mp_calvinsys_obj_t {
	mp_obj_base_t base;
  calvinsys_obj_t *obj;
} cc_mp_calvinsys_obj_t;

static mp_obj_t cc_mp_obj_write(mp_obj_t arg_obj, mp_obj_t arg_data)
{
	bool result = false;
	char *data = NULL;
	size_t size = 0;
	cc_mp_calvinsys_obj_t *mp_obj = arg_obj;

	if (encode_from_mpy_obj(&data, &size, arg_data) == CC_RESULT_SUCCESS) {
		if (mp_obj->obj != NULL) {
			if (mp_obj->obj->write(mp_obj->obj, data, size) == CC_RESULT_SUCCESS)
				result = true;
		}
	}

	return mp_obj_new_bool(result);
}

static mp_obj_t cc_mp_obj_can_read(mp_obj_t arg_obj)
{
	bool can_read = false;
	cc_mp_calvinsys_obj_t *mp_obj = arg_obj;

	if (mp_obj->obj != NULL)
		can_read = mp_obj->obj->can_read(mp_obj->obj);

	return mp_obj_new_bool(can_read);
}

static mp_obj_t cc_mp_obj_read(mp_obj_t arg_obj)
{
	mp_obj_t value = MP_OBJ_NULL;
	char *data = NULL;
	size_t size = 0;
	cc_mp_calvinsys_obj_t *mp_obj = arg_obj;

	if (mp_obj->obj != NULL) {
		if (mp_obj->obj->read(mp_obj->obj, &data, &size) == CC_RESULT_SUCCESS) {
			if (decode_to_mpy_obj(data, &value) == CC_RESULT_SUCCESS)
				return value;
		}
	}

	return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_2(cc_mp_calvinsys_obj_write, cc_mp_obj_write);
static MP_DEFINE_CONST_FUN_OBJ_1(cc_mp_calvinsys_obj_can_read, cc_mp_obj_can_read);
static MP_DEFINE_CONST_FUN_OBJ_1(cc_mp_calvinsys_obj_read, cc_mp_obj_read);

static const mp_map_elem_t cc_mp_calvinsys_obj_locals_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR_write), (mp_obj_t)&cc_mp_calvinsys_obj_write },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_can_read), (mp_obj_t)&cc_mp_calvinsys_obj_can_read },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_read), (mp_obj_t)&cc_mp_calvinsys_obj_read },
};
static MP_DEFINE_CONST_DICT(cc_mp_calvinsys_obj_locals_dict, cc_mp_calvinsys_obj_locals_table);

static const mp_obj_type_t cc_mp_calvinsys_obj_type = {
	{ &mp_type_type },
	.name = MP_QSTR_cc_mp_calvinsys_obj,
	.locals_dict = (mp_obj_t)&cc_mp_calvinsys_obj_locals_dict
};

static mp_obj_t cc_mp_calvinsys_open(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args)
{
	actor_t *actor = MP_OBJ_TO_PTR(args[0]);
  const char *name = mp_obj_str_get_str(args[1]);
  calvinsys_obj_t *obj = NULL;
	cc_mp_calvinsys_obj_t *cc_mp_calvinsys_obj = NULL;
	char buffer[400], *tmp = buffer;

	if (kw_args != NULL) {
		tmp = encode_mpy_map(&tmp, kw_args);
		if (tmp == NULL)
			return mp_const_none;
		obj = calvinsys_open(actor->calvinsys, name, buffer, tmp - buffer);
	} else
		obj = calvinsys_open(actor->calvinsys, name, NULL, 0);

	if (obj == NULL) {
	  log_error("Failed to open object");
	  return mp_const_none;
	}

	cc_mp_calvinsys_obj = m_new_obj(cc_mp_calvinsys_obj_t);
	memset(cc_mp_calvinsys_obj, 0, sizeof(cc_mp_calvinsys_obj_t));
	cc_mp_calvinsys_obj->base.type = &cc_mp_calvinsys_obj_type;
  cc_mp_calvinsys_obj->obj = obj;
  return MP_OBJ_FROM_PTR(cc_mp_calvinsys_obj);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(cc_mp_calvinsys_open_obj, 1, cc_mp_calvinsys_open);

static mp_obj_t cc_mp_calvinsys_close(mp_obj_t arg_obj)
{
	cc_mp_calvinsys_obj_t *mp_obj = arg_obj;

	calvinsys_close(mp_obj->obj);

	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(cc_mp_calvinsys_close_obj, cc_mp_calvinsys_close);

static const mp_map_elem_t cc_mp_calvinsys_globals_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_calvinsys)},
	{ MP_OBJ_NEW_QSTR(MP_QSTR_open), (mp_obj_t)&cc_mp_calvinsys_open_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_close), (mp_obj_t)&cc_mp_calvinsys_close_obj }
};

static MP_DEFINE_CONST_DICT(cc_mp_calvinsys_globals_dict, cc_mp_calvinsys_globals_table);

const mp_obj_module_t cc_mp_module_calvinsys = {
	.base = { &mp_type_module },
	.globals = (mp_obj_dict_t *)&cc_mp_calvinsys_globals_dict
};
