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
#include "actors/cc_actor_mpy.h"
#include "runtime/north/cc_port.h"
#include "runtime/north/cc_fifo.h"
#include "runtime/south/platform/cc_platform.h"
#include "micropython/py/stackctrl.h"
#include "micropython/py/gc.h"
#include "micropython/py/runtime.h"

typedef struct _mp_reader_cc_t {
  const byte *beg;
  const byte *cur;
  const byte *end;
} mp_reader_cc_t;

void nlr_jump_fail(void *val)
{
	cc_log_error("FATAL: uncaught NLR %p", val);
	mp_obj_print_exception(&mp_plat_print, (mp_obj_t)val);
  // TODO: Should return from this function
  while (1);
}

void gc_collect(void)
{
  gc_collect_start();
  gc_collect_end();
}

STATIC void stderr_print_strn(void *env, const char *str, size_t len)
{
	cc_log_error("%.*s", (int)len, str);
}

const mp_print_t mp_stderr_print = {NULL, stderr_print_strn};

mp_import_stat_t mp_import_stat(const char *path)
{
	cc_stat_t stat = CC_STAT_NO_EXIST;
  char *new_path = NULL;
  int len = strlen(CC_ACTOR_MODULES_DIR);

  if (len > 0) {
    if (cc_platform_mem_alloc((void **)&new_path, len + strlen(path) + 1) != CC_SUCCESS) {
      cc_log_error("Failed to allocate memory");
      return MP_IMPORT_STAT_NO_EXIST;
    }
    strncpy(new_path, CC_ACTOR_MODULES_DIR, strlen(CC_ACTOR_MODULES_DIR));
    strncpy(new_path + strlen(CC_ACTOR_MODULES_DIR), path, strlen(path));
    new_path[strlen(CC_ACTOR_MODULES_DIR) + strlen(path)] = '\0';
    stat = cc_platform_file_stat(new_path);
    cc_platform_mem_free(new_path);
  } else {
    new_path = (char *)path;
    stat = cc_platform_file_stat(new_path);
  }

  if (stat == CC_STAT_DIR)
		return MP_IMPORT_STAT_DIR;
	else if(stat == CC_STAT_FILE)
		return MP_IMPORT_STAT_FILE;

	return MP_IMPORT_STAT_NO_EXIST;
}

static mp_uint_t mp_reader_cc_readbyte(void *data)
{
  mp_reader_cc_t *reader = (mp_reader_cc_t *)data;

	if (reader->cur < reader->end) {
    return *reader->cur++;
  }

	return MP_READER_EOF;
}

static void mp_reader_cc_close(void *data)
{
  mp_reader_cc_t *reader = (mp_reader_cc_t *)data;

	cc_platform_mem_free((void *)reader->beg);
  m_del_obj(mp_reader_cc_t, reader);
}

void mp_reader_new_file(mp_reader_t *reader, const char *filename)
{
	mp_reader_cc_t *rm = NULL;
	char *buffer = NULL, *path = NULL;
	size_t size = 0;
  int len = strlen(CC_ACTOR_MODULES_DIR);
  cc_result_t result = CC_FAIL;

  if (len > 0) {
    if (cc_platform_mem_alloc((void **)&path, strlen(CC_ACTOR_MODULES_DIR) + strlen(filename) + 1) != CC_SUCCESS) {
      cc_log_error("Failed to allocate memory");
      return;
    }
    strncpy(path, CC_ACTOR_MODULES_DIR, strlen(CC_ACTOR_MODULES_DIR));
    strncpy(path + strlen(CC_ACTOR_MODULES_DIR), filename, strlen(filename));
    path[strlen(CC_ACTOR_MODULES_DIR) + strlen(filename)] = '\0';
    result = cc_platform_file_read(path, &buffer, &size);
    cc_platform_mem_free(path);
  } else
    result = cc_platform_file_read((char *)filename, &buffer, &size);

	if (result == CC_SUCCESS) {
    rm = m_new_obj(mp_reader_cc_t);
    rm->beg = (const byte *)buffer;
    rm->cur = (const byte *)buffer;
    rm->end = (const byte *)buffer + size;
    reader->data = rm;
    reader->readbyte = mp_reader_cc_readbyte;
    reader->close = mp_reader_cc_close;
  } else
		cc_log_error("Failed to read '%s'", path);
}

bool cc_mpy_port_init(void *heap, uint32_t heapsize, uint32_t stacksize)
{
	gc_init(heap, heap + heapsize);
	mp_init();

	return true;
}

void cc_mpy_port_deinit()
{
  mp_deinit();
}

STATIC mp_obj_t mpy_port_ccmp_tokens_available(mp_obj_t mp_actor, mp_obj_t mp_port_name, mp_obj_t mp_nbr_of_tokens)
{
	cc_actor_t*actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	int nbr_of_tokens = mp_obj_get_int(mp_nbr_of_tokens);
	cc_port_t *port = NULL;
	bool result = false;

	port = cc_port_get_from_name(actor, port_name, strlen(port_name), CC_PORT_DIRECTION_IN);
	if (port != NULL)
		result = cc_fifo_tokens_available(port->fifo, nbr_of_tokens);
	else
		cc_log_error("No port with name '%s'", port_name);

	return mp_obj_new_bool(result);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mpy_port_ccmp_tokens_available_obj, mpy_port_ccmp_tokens_available);

STATIC mp_obj_t mpy_port_ccmp_slots_available(mp_obj_t mp_actor, mp_obj_t mp_port_name, mp_obj_t mp_nbr_of_slots)
{
	cc_actor_t*actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	int nbr_of_slots = mp_obj_get_int(mp_nbr_of_slots);
	cc_port_t *port = NULL;
	bool result = false;

	port = cc_port_get_from_name(actor, port_name, strlen(port_name), CC_PORT_DIRECTION_OUT);
	if (port != NULL)
		result = cc_fifo_slots_available(port->fifo, nbr_of_slots);
	else
		cc_log_error("No port with name '%s'", port_name);

	return mp_obj_new_bool(result);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mpy_port_ccmp_slots_available_obj, mpy_port_ccmp_slots_available);

STATIC mp_obj_t mpy_port_ccmp_peek_token(mp_obj_t mp_actor, mp_obj_t mp_port_name)
{
	cc_actor_t*actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	cc_port_t *port = NULL;
	cc_token_t *token = NULL;
	mp_obj_t value = MP_OBJ_NULL;

	port = cc_port_get_from_name(actor, port_name, strlen(port_name), CC_PORT_DIRECTION_IN);
	if (port != NULL) {
		token = cc_fifo_peek(port->fifo);
		if (cc_actor_mpy_decode_to_mpy_obj(token->value, &value) == CC_SUCCESS)
			return value;
	}	else
		cc_log_error("No port with name '%s'", port_name);

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mpy_port_ccmp_peek_token_obj, mpy_port_ccmp_peek_token);

STATIC mp_obj_t mpy_port_ccmp_peek_commit(mp_obj_t mp_actor, mp_obj_t mp_port_name)
{
	cc_actor_t*actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	cc_port_t *port = NULL;
	bool value = false;

	port = cc_port_get_from_name(actor, port_name, strlen(port_name), CC_PORT_DIRECTION_IN);
	if (port != NULL) {
		cc_fifo_commit_read(port->fifo, true);
		value = true;
	} else
		cc_log_error("No port with name '%s'", port_name);

	return mp_obj_new_bool(value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mpy_port_ccmp_peek_commit_obj, mpy_port_ccmp_peek_commit);

STATIC mp_obj_t mpy_port_ccmp_write_token(mp_obj_t mp_actor, mp_obj_t mp_port_name, mp_obj_t mp_value)
{
	cc_actor_t*actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	cc_port_t *port = NULL;
	char *value = NULL;
	size_t size = 0;

	port = cc_port_get_from_name(actor, port_name, strlen(port_name), CC_PORT_DIRECTION_OUT);
	if (port != NULL) {
		if (cc_actor_mpy_encode_from_mpy_obj(mp_value, &value, &size) == CC_SUCCESS) {
			if (cc_fifo_write(port->fifo, value, size) != CC_SUCCESS) {
				cc_log_error("Failed to write token to port '%s'", port_name);
				cc_platform_mem_free((void *)value);
			}
		} else
			cc_log_error("Failed encode object");
	} else
		cc_log_error("No port with name '%s'", port_name);

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mpy_port_ccmp_write_token_obj, mpy_port_ccmp_write_token);

STATIC mp_obj_t mpy_port_ccmp_peek_cancel(mp_obj_t mp_actor, mp_obj_t mp_port_name)
{
	cc_actor_t*actor = MP_OBJ_TO_PTR(mp_actor);
	const char *port_name = mp_obj_str_get_str(mp_port_name);
	cc_port_t *port = NULL;

	port = cc_port_get_from_name(actor, port_name, strlen(port_name), CC_PORT_DIRECTION_IN);
	if (port != NULL)
		cc_fifo_cancel_commit(port->fifo);
	else
		cc_log_error("No port with name '%s'", port_name);

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mpy_port_ccmp_peek_cancel_obj, mpy_port_ccmp_peek_cancel);

STATIC const mp_map_elem_t mpy_port_globals_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_cc_mp_port)},
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_tokens_available), (mp_obj_t)&mpy_port_ccmp_tokens_available_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_slots_available), (mp_obj_t)&mpy_port_ccmp_slots_available_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_peek_token), (mp_obj_t)&mpy_port_ccmp_peek_token_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_peek_commit), (mp_obj_t)&mpy_port_ccmp_peek_commit_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_write_token), (mp_obj_t)&mpy_port_ccmp_write_token_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_ccmp_peek_cancel), (mp_obj_t)&mpy_port_ccmp_peek_cancel_obj }
};

STATIC MP_DEFINE_CONST_DICT(mp_module_mpy_port_globals, mpy_port_globals_table);

const mp_obj_module_t cc_mp_module_port = {
	.base = { &mp_type_module },
	.globals = (mp_obj_dict_t *)&mp_module_mpy_port_globals
};
