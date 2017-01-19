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
#ifndef ACTOR_MPY_H
#define ACTOR_MPY_H

#ifdef MICROPYTHON

#include "../actor.h"
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/lexer.h"

typedef struct ccmp_state_actor_t {
	mp_obj_t actor_class_instance;
	mp_obj_t actor_fire_method[2];
} ccmp_state_actor_t;

result_t actor_mpy_init_from_type(actor_t *actor, char *type, uint32_t type_len);
result_t decode_to_mpy_obj(char *buffer, mp_obj_t *value);
result_t encode_from_mpy_obj(char **buffer, size_t *size, mp_obj_t value);

#endif

#endif /* ACTOR_MPY_H */
