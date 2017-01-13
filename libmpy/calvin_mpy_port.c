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
#include <stdlib.h>
#include "../actors/actor_mpy.h"
#include "../port.h"
#include "../fifo.h"
#include "../platform.h"
#include "../micropython/py/stackctrl.h"
#include "../micropython/py/gc.h"
#include "../micropython/py/runtime.h"

long heap_size = 16 * 1024;

void nlr_jump_fail(void *val)
{
	log_error("FATAL: uncaught NLR %p", val);
	mp_obj_print_exception(&mp_plat_print, (mp_obj_t)val);
}

STATIC void stderr_print_strn(void *env, const char *str, size_t len)
{
	log_error("%.*s", (int)len, str);
}

const mp_print_t mp_stderr_print = {NULL, stderr_print_strn};

mp_import_stat_t mp_import_stat(const char *path)
{
	return 0; // return MP_IMPORT_STAT_NO_EXIST;
}

void mpy_port_init(void)
{
	// TODO: Determine how to hande micro python memory
	mp_stack_set_limit(8192);
#if MICROPY_ENABLE_GC
	char *heap = malloc(heap_size);

	gc_init(heap, heap + heap_size);
#endif
	mp_stack_ctrl_init();
	mp_init();
}

STATIC mp_obj_t mpy_port_set_callback(mp_obj_t callback_obj)
{
	MP_STATE_PORT(mpy_port_callback_obj) = callback_obj;

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mpy_port_set_callback_obj, mpy_port_set_callback);

STATIC mp_obj_t mpy_port_call_callback(void)
{
#if 1
	vstr_t vstr;

	vstr_init_len(&vstr, strlen("some_string"));
	strcpy(vstr.buf, "some_string");
	mp_obj_t obj = mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
#else
	mp_obj_t obj = mp_obj_new_str("some_string", strlen("some_string"), false);
#endif
	return mp_call_function_1(MP_STATE_PORT(mpy_port_callback_obj), obj);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mpy_port_call_callback_obj, mpy_port_call_callback);

STATIC mp_obj_t mpy_port_ccmp_tokens_available(mp_obj_t ccmp_actor, mp_obj_t action_input)
{
	mp_obj_list_t *list_o = MP_OBJ_TO_PTR(action_input);
	ccmp_actor_type_t *ptr_ccmp_actor = MP_OBJ_TO_PTR(ccmp_actor);
	mp_obj_tuple_t *tuple_o = NULL;
	bool input_ok = true;
	port_t *ccmp_port = NULL;
	uint16_t i = 0;

	for (i = 0; i < list_o->len; i++) {
		tuple_o = list_o->items[i];
		GET_STR_DATA_LEN(tuple_o->items[0], port_name, port_name_len);
		ccmp_port = port_get_from_name(ptr_ccmp_actor->actor, (char *)port_name, PORT_DIRECTION_IN);
		if (!fifo_tokens_available(&ccmp_port->fifo, 1)) {
			input_ok = false;
			break;
		}
	}
	// Make eligible for garble collectable by setting the mp_obj_t type objects to null
	list_o = MP_OBJ_NULL;
	ptr_ccmp_actor = MP_OBJ_NULL;
	tuple_o = MP_OBJ_NULL;
	log_debug("input ok: %s\n", input_ok ? "true" : "false");

	return mp_obj_new_bool(input_ok);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mpy_port_ccmp_tokens_available_obj, mpy_port_ccmp_tokens_available);

STATIC mp_obj_t mpy_port_ccmp_slots_available(mp_obj_t ccmp_actor, mp_obj_t action_output)
{
	mp_obj_list_t *list_o = MP_OBJ_TO_PTR(action_output);
	ccmp_actor_type_t *ptr_ccmp_actor = MP_OBJ_TO_PTR(ccmp_actor);
	mp_obj_tuple_t *tuple_o = NULL;
	bool output_ok = true;
	port_t *ccmp_port = NULL;

	for (uint16_t i = 0; i < list_o->len; i++) {
		tuple_o = list_o->items[i];
		GET_STR_DATA_LEN(tuple_o->items[0], port_name, port_name_len);
		ccmp_port = port_get_from_name(ptr_ccmp_actor->actor, (char *)port_name, PORT_DIRECTION_OUT);
		if (!fifo_slots_available(&ccmp_port->fifo, 1)) {
			output_ok = false;
			break;
		}
	}
	// Make eligible for garble collectable by setting the mp_obj_t type objects to null
	list_o = MP_OBJ_NULL;
	ptr_ccmp_actor = MP_OBJ_NULL;
	tuple_o = MP_OBJ_NULL;
	log_debug("output ok: %s\n", output_ok ? "true" : "false");

	return mp_obj_new_bool(output_ok);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mpy_port_ccmp_slots_available_obj, mpy_port_ccmp_slots_available);

STATIC mp_obj_t mpy_port_ccmp_peek_token(mp_obj_t ccmp_actor, mp_obj_t action_input)
{
	mp_obj_list_t *list_o = MP_OBJ_TO_PTR(action_input);
	ccmp_actor_type_t *ptr_ccmp_actor = MP_OBJ_TO_PTR(ccmp_actor);
	mp_obj_tuple_t *tuple_o = NULL;
	token_t *token = NULL;
	//uint32_t value;
	size_t repeat;
	mp_obj_t args = mp_obj_new_list(0, NULL);
	mp_obj_t retVal = MP_OBJ_NULL;
	mp_obj_t tokenList;
	mp_obj_list_t *self;
	mp_uint_t len;
	port_t *ccmp_port = NULL;

	for (uint16_t i = 0; i < list_o->len; i++) {
		tuple_o = list_o->items[i];
		GET_STR_DATA_LEN(tuple_o->items[0], port_name, port_name_len);
		ccmp_port = port_get_from_name(ptr_ccmp_actor->actor, (char *)port_name, PORT_DIRECTION_IN);
		repeat = mp_obj_get_int(tuple_o->items[1]);
		for (uint16_t j = 0; j < repeat; j++) {
			tokenList = mp_obj_new_list(0, NULL);

			// TODO: Handle failures
			token = fifo_peek(&ccmp_port->fifo);
			if (decode_to_mpy_obj(token->value, &retVal) == SUCCESS)
				mp_obj_list_append(tokenList, retVal);
		}
		self = MP_OBJ_TO_PTR(tokenList);
		len = self->len;
		if (len == 1)
			mp_obj_list_append(args, self->items[0]);
		else
			mp_obj_list_append(args, tokenList);
	}
	// Make eligible for garble collectable by setting the mp_obj_t type objects to null
	list_o = MP_OBJ_NULL;
	ptr_ccmp_actor = MP_OBJ_NULL;
	tuple_o = MP_OBJ_NULL;
	retVal = MP_OBJ_NULL;
	tokenList = MP_OBJ_NULL;
	self = MP_OBJ_NULL;

	return args;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mpy_port_ccmp_peek_token_obj, mpy_port_ccmp_peek_token);

STATIC mp_obj_t mpy_port_ccmp_peek_commit(mp_obj_t ccmp_actor, mp_obj_t action_input)
{
	mp_obj_list_t *list_in = MP_OBJ_TO_PTR(action_input);
	ccmp_actor_type_t *ptr_ccmp_actor = MP_OBJ_TO_PTR(ccmp_actor);
	mp_obj_tuple_t *tuple_in = NULL;
	uint16_t i = 0;
	port_t *ccmp_port = NULL;

	for (i = 0; i < list_in->len; i++) {
		tuple_in = list_in->items[i];
		GET_STR_DATA_LEN(tuple_in->items[0], port_name, port_name_len);
		ccmp_port = port_get_from_name(ptr_ccmp_actor->actor, (char *)port_name, PORT_DIRECTION_IN);
		fifo_commit_read(&ccmp_port->fifo);
	}
	// Make eligible for garble collectable by setting the mp_obj_t type objects to null
	list_in = MP_OBJ_NULL;
	ptr_ccmp_actor = MP_OBJ_NULL;
	tuple_in = MP_OBJ_NULL;

	return mp_obj_new_bool(true);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_2(mpy_port_ccmp_peek_commit_obj, mpy_port_ccmp_peek_commit);

STATIC mp_obj_t mpy_port_ccmp_write_token(mp_obj_t ccmp_actor, mp_obj_t action_output, mp_obj_t action_result_production)
{
	mp_obj_list_t *list_out = MP_OBJ_TO_PTR(action_output);
	mp_obj_tuple_t *tuple_out = NULL;
	mp_obj_tuple_t *a_r_prod = MP_OBJ_TO_PTR(action_result_production);
	ccmp_actor_type_t *ptr_ccmp_actor = MP_OBJ_TO_PTR(ccmp_actor);
	size_t repeat;
	mp_obj_t *retval = mp_obj_new_list(0, NULL);
	int32_t i = 0;
	token_t token;
	port_t *ccmp_port = NULL;
	mp_obj_list_t *temp_list = NULL;

	for (i = 0; i < list_out->len; i++) {
		tuple_out = list_out->items[i];
		GET_STR_DATA_LEN(tuple_out->items[0], port_name, port_name_len);
		ccmp_port = port_get_from_name(ptr_ccmp_actor->actor, (char *)port_name, PORT_DIRECTION_OUT);
		repeat = mp_obj_get_int(tuple_out->items[1]);
		if (repeat == 1)
			mp_obj_list_append(retval, a_r_prod->items[0]);
		else
			retval = a_r_prod->items[0];
		temp_list = MP_OBJ_TO_PTR(retval);
		for (i = 0; i < temp_list->len; i++) {
			if (encode_from_mpy_obj(&token.value, &token.size, temp_list->items[i]) == SUCCESS) {
				if (fifo_write(&ccmp_port->fifo, token.value, token.size) == SUCCESS) {
					platform_mem_free(token.value);
					token.value = NULL;
				}
			}
		}
	}
	// Make eligible for garble collectable by setting the mp_obj_t type objects to null
	list_out = MP_OBJ_NULL;
	tuple_out = MP_OBJ_NULL;
	a_r_prod = MP_OBJ_NULL;
	ptr_ccmp_actor = MP_OBJ_NULL;
	retval = MP_OBJ_NULL;
	temp_list = MP_OBJ_NULL;

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mpy_port_ccmp_write_token_obj, mpy_port_ccmp_write_token);

STATIC mp_obj_t mpy_port_ccmp_peek_cancel(mp_obj_t ccmp_actor, mp_obj_t action_input)
{
	mp_obj_list_t *list_in = MP_OBJ_TO_PTR(action_input);
	ccmp_actor_type_t *ptr_ccmp_actor = MP_OBJ_TO_PTR(ccmp_actor);
	mp_obj_tuple_t *tuple_in = NULL;
	uint16_t i = 0;
	port_t *ccmp_port = NULL;

	for (i = 0; i < list_in->len; i++) {
		tuple_in = list_in->items[i];
		GET_STR_DATA_LEN(tuple_in->items[0], port_name, port_name_len);
		ccmp_port = port_get_from_name(ptr_ccmp_actor->actor, (char *)port_name, PORT_DIRECTION_IN);
		fifo_cancel_commit(&ccmp_port->fifo);
	}
	// Make eligible for garble collectable by setting the mp_obj_t type objects to null
	list_in = MP_OBJ_NULL;
	ptr_ccmp_actor = MP_OBJ_NULL;
	tuple_in = MP_OBJ_NULL;

	return mp_obj_new_bool(true);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mpy_port_ccmp_peek_cancel_obj, mpy_port_ccmp_peek_cancel);

STATIC const mp_map_elem_t mpy_port_globals_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_mpy_port)},
	{ MP_OBJ_NEW_QSTR(MP_QSTR_set_callback), (mp_obj_t)&mpy_port_set_callback_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_call_callback), (mp_obj_t)&mpy_port_call_callback_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_tokens_available), (mp_obj_t)&mpy_port_ccmp_tokens_available_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_slots_available), (mp_obj_t)&mpy_port_ccmp_slots_available_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_peek_token), (mp_obj_t)&mpy_port_ccmp_peek_token_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_peek_commit), (mp_obj_t)&mpy_port_ccmp_peek_commit_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_write_token), (mp_obj_t)&mpy_port_ccmp_write_token_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_peek_cancel), (mp_obj_t)&mpy_port_ccmp_peek_cancel_obj }
};

STATIC MP_DEFINE_CONST_DICT(mp_module_mpy_port_globals, mpy_port_globals_table);

const mp_obj_module_t mp_module_mpy_port = {
	.base = { &mp_type_module },
	.globals = (mp_obj_dict_t *)&mp_module_mpy_port_globals
};
