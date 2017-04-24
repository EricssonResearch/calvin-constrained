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
#include "../runtime/south/platform/cc_platform.h"
#include "../runtime/north/cc_actor.h"

typedef struct cc_mp_environmental_t {
	mp_obj_base_t base;
	calvinsys_sensors_environmental_t *environmental;
} cc_mp_environmental_t;

static mp_obj_t environmental_get_temperature(mp_obj_t self_in)
{
	cc_mp_environmental_t *environmental = self_in;
	double temperature = 0;

	if (environmental->environmental->get_temperature(&temperature) != SUCCESS) {
		log_error("Failed to get temperature");
		return mp_const_none;
	}

	return mp_obj_new_float((mp_float_t)(temperature));
}
static MP_DEFINE_CONST_FUN_OBJ_1(environmental_obj_get_temperature, environmental_get_temperature);

static const mp_map_elem_t environmental_locals_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR_get_temperature), (mp_obj_t)&environmental_obj_get_temperature }
};
static MP_DEFINE_CONST_DICT(environmental_locals_dict, environmental_locals_table);

static const mp_obj_type_t environmental_type = {
	{ &mp_type_type },
	.name = MP_QSTR_Environmental,
	.locals_dict = (mp_obj_dict_t *)&environmental_locals_dict
};

static mp_obj_t environmental_register(mp_obj_t mp_actor)
{
	static cc_mp_environmental_t *environmental;
	actor_t *actor = MP_OBJ_TO_PTR(mp_actor);

	log("Actor name: %s", actor->type);

	if (environmental == NULL) {
		environmental = m_new_obj(cc_mp_environmental_t);
		memset(environmental, 0, sizeof(cc_mp_environmental_t));
		environmental->base.type = &environmental_type;
		environmental->environmental = (calvinsys_sensors_environmental_t *)list_get(actor->calvinsys, "calvinsys.sensors.environmental");
		if (environmental->environmental == NULL) {
			log_error("'calvinsys.sensors.environmental' is not supported");
			return mp_const_none;
		}
	}

	return MP_OBJ_FROM_PTR(environmental);
}
static MP_DEFINE_CONST_FUN_OBJ_1(environmental_register_obj, environmental_register);

static const mp_map_elem_t environmental_globals_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_environmental)},
	{ MP_OBJ_NEW_QSTR(MP_QSTR_register), (mp_obj_t)&environmental_register_obj }
};

static MP_DEFINE_CONST_DICT(environmental_globals_dict, environmental_globals_table);

const mp_obj_module_t mp_module_environmental = {
	.base = { &mp_type_module },
	.globals = (mp_obj_dict_t *)&environmental_globals_dict
};
