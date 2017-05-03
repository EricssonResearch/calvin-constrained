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
#include "../actors/cc_actor_mpy.h"
#include "../runtime/north/cc_port.h"
#include "../runtime/north/cc_fifo.h"
#include "../runtime/south/platform/cc_platform.h"
#include "../micropython/py/stackctrl.h"
#include "../micropython/py/gc.h"
#include "../micropython/py/runtime.h"

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

bool mpy_port_init(uint32_t heap_size)
{
	void *heap = NULL;

	if (platform_mem_alloc(&heap, heap_size) != CC_RESULT_SUCCESS) {
		log_error("Failed allocate MicroPython heap");
		return false;
	}

	mp_stack_set_limit(8192);
	mp_stack_ctrl_init();
	gc_init(heap, heap + heap_size);
	mp_init();

	return true;
}

STATIC mp_obj_t mpy_port_ccmp_tokens_available(mp_obj_t mp_actor, mp_obj_t mp_port_name, mp_obj_t mp_nbr_of_tokens)
{
	actor_t *actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	int nbr_of_tokens = mp_obj_get_int(mp_nbr_of_tokens);
	port_t *port = NULL;
	bool result = false;

	port = port_get_from_name(actor, port_name, PORT_DIRECTION_IN);
	if (port != NULL)
		result = fifo_tokens_available(&port->fifo, nbr_of_tokens);
	else
		log_error("No port with name '%s'", port_name);

	return mp_obj_new_bool(result);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mpy_port_ccmp_tokens_available_obj, mpy_port_ccmp_tokens_available);

STATIC mp_obj_t mpy_port_ccmp_slots_available(mp_obj_t mp_actor, mp_obj_t mp_port_name, mp_obj_t mp_nbr_of_slots)
{
	actor_t *actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	int nbr_of_slots = mp_obj_get_int(mp_nbr_of_slots);
	port_t *port = NULL;
	bool result = false;

	port = port_get_from_name(actor, port_name, PORT_DIRECTION_OUT);
	if (port != NULL)
		result = fifo_slots_available(&port->fifo, nbr_of_slots);
	else
		log_error("No port with name '%s'", port_name);

	return mp_obj_new_bool(result);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mpy_port_ccmp_slots_available_obj, mpy_port_ccmp_slots_available);

STATIC mp_obj_t mpy_port_ccmp_peek_token(mp_obj_t mp_actor, mp_obj_t mp_port_name)
{
	actor_t *actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	port_t *port = NULL;
	token_t *token = NULL;
	mp_obj_t value = MP_OBJ_NULL;

	port = port_get_from_name(actor, port_name, PORT_DIRECTION_IN);
	if (port != NULL) {
		token = fifo_peek(&port->fifo);
		if (decode_to_mpy_obj(token->value, &value) == CC_RESULT_SUCCESS)
			return value;
	}	else
		log_error("No port with name '%s'", port_name);

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mpy_port_ccmp_peek_token_obj, mpy_port_ccmp_peek_token);

STATIC mp_obj_t mpy_port_ccmp_peek_commit(mp_obj_t mp_actor, mp_obj_t mp_port_name)
{
	actor_t *actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	port_t *port = NULL;
	bool value = false;

	port = port_get_from_name(actor, port_name, PORT_DIRECTION_IN);
	if (port != NULL)
		value = fifo_commit_read(&port->fifo);
	else
		log_error("No port with name '%s'", port_name);

	return mp_obj_new_bool(value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mpy_port_ccmp_peek_commit_obj, mpy_port_ccmp_peek_commit);

STATIC mp_obj_t mpy_port_ccmp_write_token(mp_obj_t mp_actor, mp_obj_t mp_port_name, mp_obj_t mp_value)
{
	actor_t *actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	port_t *port = NULL;
	token_t token;

	port = port_get_from_name(actor, port_name, PORT_DIRECTION_OUT);
	if (port != NULL) {
		if (encode_from_mpy_obj(&token.value, &token.size, mp_value) == CC_RESULT_SUCCESS) {
			if (fifo_write(&port->fifo, token.value, token.size) != CC_RESULT_SUCCESS)
				log_error("Failed to write token to port '%s'", port_name);
			free_token(&token);
		}
	} else
		log_error("No port with name '%s'", port_name);

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mpy_port_ccmp_write_token_obj, mpy_port_ccmp_write_token);

STATIC mp_obj_t mpy_port_ccmp_peek_cancel(mp_obj_t mp_actor, mp_obj_t mp_port_name)
{
	actor_t *actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	port_t *port = NULL;

	port = port_get_from_name(actor, port_name, PORT_DIRECTION_IN);
	if (port != NULL)
		fifo_cancel_commit(&port->fifo);
	else
		log_error("No port with name '%s'", port_name);

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mpy_port_ccmp_peek_cancel_obj, mpy_port_ccmp_peek_cancel);

STATIC const mp_map_elem_t mpy_port_globals_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_mpy_port)},
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
