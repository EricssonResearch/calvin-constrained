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
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "cc_actor_mpy.h"
#include "cc_mpy_common.h"
#include "runtime/north/cc_actor.h"
#include "runtime/south/platform/cc_platform.h"
#include "runtime/north/coder/cc_coder.h"
#include "py/frozenmod.h"
#include "py/gc.h"

typedef struct cc_actor_mpy_state_t {
	mp_obj_t actor_class_instance;
	mp_obj_t actor_fire_method[2];
} cc_actor_mpy_state_t;

static cc_result_t cc_actor_mpy_init(cc_actor_t *actor, cc_list_t *managed_attributes)
{
	uint32_t j = 0, nbr_of_attributes = cc_list_count(managed_attributes);
	cc_list_t *list = managed_attributes;
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t args[2 + 2 * nbr_of_attributes];
	mp_obj_t tmp = MP_OBJ_NULL;
	cc_result_t result = CC_SUCCESS;
	int i = 0;
	mp_obj_t actor_init_method[2];

	mp_load_method(state->actor_class_instance, QSTR_FROM_STR_STATIC("init"), actor_init_method);

	args[0] = actor_init_method[0];
	args[1] = actor_init_method[1];
/*
	py_managed_list = mp_obj_new_list(nbr_of_attributes, NULL);
	mp_store_attr(state->actor_class_instance, QSTR_FROM_STR_STATIC("_managed"), py_managed_list);
*/
	while (list != NULL && result == CC_SUCCESS) {
		tmp = mp_obj_new_str(list->id, list->id_len);
		j += 2;
		args[j] = tmp;
		if (cc_mpy_decode_to_mpy_obj(list->data, &tmp) != CC_SUCCESS) {
			cc_log_error("Failed to decode value from managed attribute");
			return CC_FAIL;
		}
		args[j + 1] = tmp;
		cc_log_debug("Added managed attribute '%s'", list->id);
		list = list->next;
	}

	// TODO catch exception
	mp_call_method_n_kw(0, nbr_of_attributes, args);

	for (i = 0; i < j; i++)
		args[i] = MP_OBJ_NULL;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_mpy_set_state(cc_actor_t *actor, cc_list_t *managed_attributes)
{
	uint32_t nbr_of_attributes = cc_list_count(managed_attributes);
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	uint32_t index = 0;
	cc_list_t *list = managed_attributes;
	mp_obj_t value = MP_OBJ_NULL;
	mp_obj_t py_managed_list = MP_OBJ_NULL;
	qstr q_attr;
	mp_obj_t attr = MP_OBJ_NULL;

	// Create the _managed list and the managed attributes on the Python instance
	py_managed_list = mp_obj_new_list(nbr_of_attributes, NULL);
	mp_store_attr(state->actor_class_instance, QSTR_FROM_STR_STATIC("_managed"), py_managed_list);
	while (list != NULL) {
		q_attr = qstr_from_strn(list->id, list->id_len);
		attr = mp_obj_new_str(list->id, list->id_len);
		if (cc_mpy_decode_to_mpy_obj((char *)list->data, &value) != CC_SUCCESS) {
			cc_log_error("Failed to decode value from managed attribute");
			return CC_FAIL;
		}
		mp_obj_list_store(py_managed_list, MP_OBJ_NEW_SMALL_INT(index++), attr);
		mp_store_attr(state->actor_class_instance, q_attr, value);
		list = list->next;
	}

	value = MP_OBJ_NULL;
	py_managed_list = MP_OBJ_NULL;
	attr = MP_OBJ_NULL;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_mpy_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	qstr q_attr;
	mp_obj_t mpy_attr[2];
	mp_obj_list_t *managed_list = NULL;
	uint32_t i = 0;
	size_t size = 0;
	char *packed_value = NULL;

	mp_load_method(state->actor_class_instance, QSTR_FROM_STR_STATIC("_managed"), mpy_attr);
	if (mpy_attr[0] == MP_OBJ_NULL && mpy_attr[1] == MP_OBJ_NULL) {
		cc_log_debug("No managed variables");
		return CC_SUCCESS;
	}

	managed_list = MP_OBJ_TO_PTR(mpy_attr[0]);
	for (i = 0; i < managed_list->len; i++) {
		if (!MP_OBJ_IS_STR(managed_list->items[i])) {
			cc_log_error("Attribute name is not a string");
			return CC_FAIL;
		}

		const char *attribute_name = mp_obj_str_get_str(managed_list->items[i]);

		q_attr = qstr_from_strn(attribute_name, strlen(attribute_name));

		mp_load_method(state->actor_class_instance, q_attr, mpy_attr);
		if (mpy_attr[0] == MP_OBJ_NULL) {
			cc_log_error("Failed to get managed attribute");
			return CC_FAIL;
		}

		if (cc_mpy_encode_from_mpy_obj(mpy_attr[0], &packed_value, &size, true) != CC_SUCCESS) {
			cc_log_error("Failed to encode attribute");
			return CC_FAIL;
		}

		if (cc_list_add_n(managed_attributes, attribute_name, strlen(attribute_name), packed_value, size) == NULL) {
			cc_log_error("Failed to add attribute");
			cc_platform_mem_free(packed_value);
			return CC_FAIL;
		}
	}

	return CC_SUCCESS;
}

static cc_result_t cc_actor_mpy_get_requires(cc_actor_t *actor, cc_list_t **requires)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t mpy_attr[2];
	mp_obj_list_t *requires_list = NULL;
	uint32_t i = 0;

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("requires"), mpy_attr);
	if (mpy_attr[0] == MP_OBJ_NULL && mpy_attr[1] == MP_OBJ_NULL)
		return CC_SUCCESS;

	requires_list = MP_OBJ_TO_PTR(mpy_attr[0]);
	for (i = 0; i < requires_list->len; i++) {
		if (!MP_OBJ_IS_STR(requires_list->items[i])) {
			cc_log_error("Requirement is not a string");
			return CC_FAIL;
		}

		const char *name = mp_obj_str_get_str(requires_list->items[i]);

		if (cc_list_add_n(requires, name, strlen(name), NULL, 0) == NULL) {
			cc_log_error("Failed to add requirement");
			return CC_FAIL;
		}
	}

	return CC_SUCCESS;
}

static bool cc_actor_mpy_fire(cc_actor_t *actor)
{
	mp_obj_t res;
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	bool did_fire = false;

	res = mp_call_method_n_kw(0, 0, state->actor_fire_method);
	if (res != mp_const_none && mp_obj_is_true(res))
		did_fire = true;
	res = MP_OBJ_NULL;

	gc_collect();
#ifdef CC_DEBUG_MEM
	gc_dump_info();
#endif

	return did_fire;
}

static void cc_actor_mpy_free_state(cc_actor_t *actor)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;

	if (state != NULL) {
		// TODO: gc_collect does not free objects, fix it.
		state->actor_class_instance = MP_OBJ_NULL;
		cc_platform_mem_free((void *)state);
		actor->instance_state = NULL;
	}

	gc_collect();
#ifdef CC_DEBUG_MEM
	gc_dump_info();
#endif
}

static void cc_actor_mpy_will_migrate(cc_actor_t *actor)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t mpy_will_migrate[2];

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("will_migrate"), mpy_will_migrate);
	if (mpy_will_migrate[0] != MP_OBJ_NULL && mpy_will_migrate[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, mpy_will_migrate);

	mpy_will_migrate[0] = MP_OBJ_NULL;
	mpy_will_migrate[1] = MP_OBJ_NULL;
}

static void cc_actor_mpy_will_end(cc_actor_t *actor)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t mpy_will_end[2];

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("will_end"), mpy_will_end);
	if (mpy_will_end[0] != MP_OBJ_NULL && mpy_will_end[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, mpy_will_end);

	mpy_will_end[0] = MP_OBJ_NULL;
	mpy_will_end[1] = MP_OBJ_NULL;
}

static void cc_actor_mpy_did_migrate(cc_actor_t *actor)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t mpy_did_migrate[2];

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("did_migrate"), mpy_did_migrate);
	if (mpy_did_migrate[0] != MP_OBJ_NULL && mpy_did_migrate[1] != MP_OBJ_NULL)
		mp_call_method_n_kw(0, 0, mpy_did_migrate);

	mpy_did_migrate[0] = MP_OBJ_NULL;
	mpy_did_migrate[1] = MP_OBJ_NULL;
}

static void cc_actor_mpy_did_replicate(cc_actor_t *actor, uint32_t index)
{
	cc_actor_mpy_state_t *state = (cc_actor_mpy_state_t *)actor->instance_state;
	mp_obj_t mpy_did_replicate[3];

	mp_load_method_maybe(state->actor_class_instance, QSTR_FROM_STR_STATIC("did_replicate"), mpy_did_replicate);
	if (mpy_did_replicate[0] != MP_OBJ_NULL && mpy_did_replicate[1] != MP_OBJ_NULL) {
		mpy_did_replicate[2] = MP_OBJ_NEW_SMALL_INT(index);
		mp_call_method_n_kw(1, 0, mpy_did_replicate);
	}

	mpy_did_replicate[0] = MP_OBJ_NULL;
	mpy_did_replicate[1] = MP_OBJ_NULL;
	mpy_did_replicate[2] = MP_OBJ_NULL;
}

char *cc_actor_mpy_get_path_from_type(char *type, uint32_t type_len, const char *extension, bool add_modules_dir)
{
	char *path = NULL;
	int i = 0, len = 0;

	if (add_modules_dir) {
	  len = strlen(CC_ACTOR_MODULES_DIR) + 7 + type_len + strlen(extension) + 1; // 7 for actors/
	  if (cc_platform_mem_alloc((void **)&path, len) != CC_SUCCESS) {
	    cc_log_error("Failed to allocate memory");
	    return NULL;
	  }
		memset(path, 0, len);
	  strncpy(path, CC_ACTOR_MODULES_DIR, strlen(CC_ACTOR_MODULES_DIR));
		strncpy(path + strlen(path), "actors/", 7);
	  strncpy(path + strlen(path), type, type_len);
	} else {
		len = 7 + type_len + strlen(extension) + 1; // 7 for actors/
		if (cc_platform_mem_alloc((void **)&path, len) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return NULL;
		}
		memset(path, 0, len);
		strncpy(path + strlen(path), "actors/", 7);
		strncpy(path + strlen(path), type, type_len);
	}

	while (i < len) {
		if (path[i] == '.')
			path[i] = '/';
		i++;
	}

	strncpy(path + strlen(path), extension, strlen(extension));

	return path;
}

cc_result_t cc_actor_mpy_init_from_type(cc_actor_t *actor)
{
	char *type = NULL, *class = NULL, instance_name[30];
	qstr actor_type_qstr, actor_class_qstr;
	mp_obj_t args[2];
	cc_actor_mpy_state_t *state = NULL;
	mp_obj_t actor_module;
	mp_obj_t actor_class_ref[2];
	static int counter;
	uint8_t pos = 0, class_len = 0;

	cc_log("Loading Python actor '%s'", actor->type);

	sprintf(instance_name, "actor_obj%d", counter++);

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_mpy_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	memset(state, 0, sizeof(cc_actor_mpy_state_t));
	actor->instance_state = (void *)state;

	// + 7 for actor/
	if (cc_platform_mem_alloc((void **)&type, strlen(actor->type) + 7 + 1) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	memset(type, 0, 7 + strlen(actor->type) + 1);
	strncpy(type, "actors.", 7);
	strncpy(type + strlen(type), actor->type, strlen(actor->type));
	pos = strlen(type);

	// get the class name (the part after the last "." in the passed module type string
	while (pos-- > 0) {
			if (type[pos] == '.') {
				class = type + (pos + 1);
				break;
			}
			class_len++;
	}

	if (class == NULL) {
		cc_log_error("Failed to get python class name from '%s'", type);
		cc_platform_mem_free(state);
		cc_platform_mem_free(type);
		return CC_FAIL;
	}

	actor_type_qstr = qstr_from_strn(type, strlen(type));
	actor_class_qstr = qstr_from_strn(class, class_len);

	// load the module
	// you have to pass mp_const_true to the second argument in order to return reference
	// to the module instance, otherwise it will return a reference to the top-level package
	actor_module = mp_import_name(actor_type_qstr, mp_const_true, MP_OBJ_NEW_SMALL_INT(0));
	if (actor_module != MP_OBJ_NULL) {
		mp_store_global(qstr_from_strn(type, strlen(type)), actor_module);
	} else {
		cc_log_error("Failed to import '%s'", type);
		cc_platform_mem_free(state);
		cc_platform_mem_free(type);
		return CC_FAIL;
	}

	// load the class
	mp_load_method(actor_module, actor_class_qstr, actor_class_ref);

	// pass the actor and calvinsys as a mp_obj_t
	args[0] = MP_OBJ_FROM_PTR(actor);
	args[1] = MP_OBJ_FROM_PTR(actor->calvinsys);
	state->actor_class_instance = mp_call_function_n_kw(actor_class_ref[0], 2, 0, args);
	mp_store_name(QSTR_FROM_STR_STATIC(instance_name), state->actor_class_instance);

	// load the fire method
	mp_load_method(state->actor_class_instance, QSTR_FROM_STR_STATIC("fire"), state->actor_fire_method);

	actor_module = MP_OBJ_NULL;
	actor_class_ref[0] = MP_OBJ_NULL;
	actor_class_ref[1] = MP_OBJ_NULL;

	actor->init = cc_actor_mpy_init;
	actor->fire = cc_actor_mpy_fire;
	actor->set_state = cc_actor_mpy_set_state;
	actor->get_managed_attributes = cc_actor_mpy_get_attributes;
	actor->free_state = cc_actor_mpy_free_state;
	actor->will_migrate = cc_actor_mpy_will_migrate;
	actor->will_end = cc_actor_mpy_will_end;
	actor->did_migrate = cc_actor_mpy_did_migrate;
	actor->did_replicate = cc_actor_mpy_did_replicate;
	actor->get_requires = cc_actor_mpy_get_requires;

	cc_platform_mem_free(type);

	return CC_SUCCESS;
}

bool cc_actor_mpy_has_module(char *type)
{
	char *path = NULL;
	bool found = false;

	path = cc_actor_mpy_get_path_from_type(type, strlen(type), ".py", false);
	if (path != NULL) {
		found = mp_frozen_stat(path) == MP_IMPORT_STAT_FILE;
		cc_platform_mem_free(path);
		if (found)
			return true;
	}

	path = cc_actor_mpy_get_path_from_type(type, strlen(type), ".mpy", true);
	if (path != NULL) {
		found = cc_platform_file_stat(path) == CC_STAT_FILE;
		cc_platform_mem_free(path);
	}

	return found;
}
