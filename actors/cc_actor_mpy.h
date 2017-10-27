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
#ifndef CC_ACTOR_MPY_H
#define CC_ACTOR_MPY_H

#ifdef CC_PYTHON_ENABLED

#include "runtime/north/cc_actor.h"
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/lexer.h"

typedef struct cc_actor_mpy_state_t {
	mp_obj_t actor_class_instance;
	mp_obj_t actor_fire_method[2];
} cc_actor_mpy_state_t;

cc_result_t cc_actor_mpy_decode_to_mpy_obj(char *buffer, mp_obj_t *value);
cc_result_t cc_actor_mpy_encode_from_mpy_obj(mp_obj_t input, char **buffer, size_t *size);
char *cc_actor_mpy_get_path_from_type(char *type, uint32_t type_len, const char *extension, bool add_modules_dir);
cc_result_t cc_actor_mpy_init_from_type(cc_actor_t *actor);
bool cc_actor_mpy_has_module(char *type);

#endif

#endif /* CC_ACTOR_MPY_H */
