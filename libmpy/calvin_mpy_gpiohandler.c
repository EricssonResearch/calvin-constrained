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
#include "../platform.h"
#include "../actor.h"
#include "../node.h"

typedef struct cc_mp_gpiopin_t {
	mp_obj_base_t base;
	uint32_t pin;
	calvin_ingpio_t *gpio;
	calvinsys_io_giohandler_t *gpiohandler;
} cc_mp_gpiopin_t;

typedef struct cc_mp_gpiohandler_t {
	mp_obj_base_t base;
} cc_mp_gpiohandler_t;

static mp_obj_t gpiopin_edge_detected(mp_obj_t self_in)
{
	cc_mp_gpiopin_t *gpio = self_in;

	if (gpio->gpio->has_triggered) {
		gpio->gpio->has_triggered = false;
		return mp_const_true;
	}

	return mp_const_false;
}

static mp_obj_t gpiopin_set_state(mp_obj_t self_in, mp_obj_t state)
{
	cc_mp_gpiopin_t *gpio = self_in;

	gpio->gpiohandler->set_gpio(gpio->pin, mp_obj_get_int(state));

	return mp_const_none;
}

static mp_obj_t gpiopin_edge_value(mp_obj_t self_in)
{
	cc_mp_gpiopin_t *gpio = self_in;

	log("Getting gpio value");

	return mp_obj_new_int(gpio->gpio->value);
}

static mp_obj_t gpiopin_close(mp_obj_t self_in)
{
	cc_mp_gpiopin_t *gpio = self_in;

	if (gpio->gpio != NULL)
		gpio->gpiohandler->uninit_gpio(gpio->gpiohandler, gpio->pin, GPIO_IN);
	else
		gpio->gpiohandler->uninit_gpio(gpio->gpiohandler, gpio->pin, GPIO_OUT);

	gpio = MP_OBJ_NULL;

	return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_1(gpiopin_obj_edge_detected, gpiopin_edge_detected);
static MP_DEFINE_CONST_FUN_OBJ_2(gpiopin_obj_set_state, gpiopin_set_state);
static MP_DEFINE_CONST_FUN_OBJ_1(gpiopin_obj_edge_value, gpiopin_edge_value);
static MP_DEFINE_CONST_FUN_OBJ_1(gpiopin_obj_close, gpiopin_close);

static const mp_map_elem_t gpiopin_locals_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR_edge_detected), (mp_obj_t)&gpiopin_obj_edge_detected },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_set_state), (mp_obj_t)&gpiopin_obj_set_state },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_edge_value), (mp_obj_t)&gpiopin_obj_edge_value },
	{ MP_OBJ_NEW_QSTR(MP_QSTR_close), (mp_obj_t)&gpiopin_obj_close },
};
static MP_DEFINE_CONST_DICT(gpiopin_locals_dict, gpiopin_locals_table);

static const mp_obj_type_t gpiopin_type = {
	{ &mp_type_type },
	.name = MP_QSTR_GPIOPin,
	.locals_dict = (mp_obj_t)&gpiopin_locals_dict
};

static mp_obj_t gpiohandler_open(mp_uint_t n_args, const mp_obj_t *args)
{
	cc_mp_gpiopin_t *gpiopin = NULL;
	uint32_t pin = 0;
	char *dir = NULL, *pull = NULL, *edge = NULL;
	mp_uint_t len;
	node_t *node = node_get();
	calvinsys_io_giohandler_t *gpiohandler = NULL;

	gpiohandler = (calvinsys_io_giohandler_t *)list_get(node->calvinsys, "calvinsys.io.gpiohandler");
	if (gpiohandler == NULL) {
		log_error("calvinsys.io.gpiohandler is not supported");
		return mp_const_none;
	}

	pin = mp_obj_get_int(args[1]);
	dir = (char *)mp_obj_str_get_data(args[2], &len);
	if (dir[0] == 'o') {
		if (gpiohandler->init_out_gpio(pin) == SUCCESS) {
			gpiopin = m_new_obj(cc_mp_gpiopin_t);
			memset(gpiopin, 0, sizeof(cc_mp_gpiopin_t));
			gpiopin->base.type = &gpiopin_type;
			gpiopin->pin = pin;
			gpiopin->gpiohandler = gpiohandler;
			return MP_OBJ_FROM_PTR(gpiopin);
		} else
			log_error("Failed to initialize gpio");
	} else if (dir[0] == 'i') {
		if (n_args == 5) {
			gpiopin = m_new_obj(cc_mp_gpiopin_t);
			memset(gpiopin, 0, sizeof(cc_mp_gpiopin_t));
			gpiopin->base.type = &gpiopin_type;
			gpiopin->pin = pin;
			gpiopin->gpiohandler = gpiohandler;
			pull = (char *)mp_obj_str_get_data(args[3], &len);
			edge = (char *)mp_obj_str_get_data(args[4], &len);
			gpiopin->gpio = gpiohandler->init_in_gpio(gpiohandler, pin, pull[0], edge[0]);
			return MP_OBJ_FROM_PTR(gpiopin);
		} else
			log_error("Missing argument(s)");
	} else
		log_error("Unsupported direction");

	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(gpiohandler_obj_open, 3, 5, gpiohandler_open);

static const mp_map_elem_t gpiohandler_locals_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR_open), (mp_obj_t)&gpiohandler_obj_open }
};
static MP_DEFINE_CONST_DICT(gpiohandler_locals_dict, gpiohandler_locals_table);

static const mp_obj_type_t gpiohandler_type = {
	{ &mp_type_type },
	.name = MP_QSTR_GPIOHandler,
	.locals_dict = (mp_obj_dict_t *)&gpiohandler_locals_dict
};

static mp_obj_t gpiohandler_register()
{
	static cc_mp_gpiohandler_t *gpiohandler;

	if (gpiohandler == NULL) {
		gpiohandler = m_new_obj(cc_mp_gpiohandler_t);
		memset(gpiohandler, 0, sizeof(cc_mp_gpiohandler_t));
		gpiohandler->base.type = &gpiohandler_type;
	}

	return MP_OBJ_FROM_PTR(gpiohandler);
}
static MP_DEFINE_CONST_FUN_OBJ_0(gpiohandler_register_obj, gpiohandler_register);

static const mp_map_elem_t gpiohandler_globals_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_gpiohandler)},
	{ MP_OBJ_NEW_QSTR(MP_QSTR_register), (mp_obj_t)&gpiohandler_register_obj }
};

static MP_DEFINE_CONST_DICT(gpiohandler_globals_dict, gpiohandler_globals_table);

const mp_obj_module_t mp_module_gpiohandler = {
	.base = { &mp_type_module },
	.globals = (mp_obj_dict_t *)&gpiohandler_globals_dict
};
