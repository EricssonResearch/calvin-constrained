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
#include "runtime/north/cc_common.h"
#include "runtime/north/coder/cc_coder.h"
#include "runtime/north/cc_actor.h"
#include "runtime/south/platform/cc_platform.h"
#include "cc_calvinsys.h"

cc_result_t cc_calvinsys_create_capability(cc_calvinsys_t *calvinsys, const char *name, cc_result_t (*open)(cc_calvinsys_obj_t*, char*, size_t), cc_result_t (*deserialize)(cc_calvinsys_obj_t*, char*), void *state)
{
	cc_calvinsys_capability_t *capability = NULL;
	cc_list_t *item = NULL;

	if (cc_list_get(calvinsys->capabilities, name) != NULL) {
		cc_log_error("Capability '%s' already registered", name);
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&capability, sizeof(cc_calvinsys_capability_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	item = cc_list_add_n(&calvinsys->capabilities, name, strlen(name), capability, sizeof(cc_calvinsys_capability_t));
	if (item == NULL) {
		cc_platform_mem_free((void *)capability);
		return CC_FAIL;
	}

	capability->open = open;
	capability->deserialize = deserialize;
	capability->calvinsys = calvinsys;
	capability->state = state;
	capability->name = item->id;

	return CC_SUCCESS;
}

void cc_calvinsys_delete_capability(cc_calvinsys_t *calvinsys, const char *name)
{
	cc_calvinsys_capability_t *capability = NULL;
	cc_list_t *item = NULL;

	item = cc_list_get(calvinsys->capabilities, name);
	if (item != NULL) {
		capability = (cc_calvinsys_capability_t *)item->data;
		if (capability->state != NULL)
			cc_platform_mem_free(capability->state);
		cc_list_remove(&calvinsys->capabilities, name);
		cc_platform_mem_free((void *)capability);
	}
}

char *cc_calvinsys_open(cc_actor_t *actor, const char *name, char *data, size_t size)
{
	cc_calvinsys_capability_t *capability = NULL;
	cc_calvinsys_obj_t *obj = NULL;
	char id[CC_UUID_BUFFER_SIZE];
	cc_list_t *item = NULL;

	item = cc_list_get(actor->calvinsys->capabilities, name);
	if (item == NULL) {
		cc_log_error("Capability '%s' not registered", name);
		return NULL;
	}

	capability = (cc_calvinsys_capability_t *)item->data;

	if (capability->open == NULL) {
		cc_log_error("Capability does not have a open method");
		return NULL;
	}

	if (cc_platform_mem_alloc((void **)&obj, sizeof(cc_calvinsys_obj_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	obj->can_write = NULL;
	obj->write = NULL;
	obj->can_read = NULL;
	obj->read = NULL;
	obj->close = NULL;
	obj->serialize = NULL;
	obj->state = NULL;
	obj->capability = capability;
	obj->actor = actor;

	cc_gen_uuid(id, NULL);
	item = cc_list_add_n(&actor->calvinsys->objects, id, strlen(id) + 1, obj, sizeof(cc_calvinsys_t));
	if (item == NULL) {
		cc_log_error("Failed to add '%s'", name);
		cc_platform_mem_free((void *)obj);
		return NULL;
	}
	obj->id = item->id;

	if (capability->open(obj, data, size) != CC_SUCCESS) {
		cc_log_error("Failed to open '%s'", name);
		cc_platform_mem_free((void *)obj);
		return NULL;
	}

	cc_log("calvinsys: Opened '%s', capability '%s'", item->id, name);

	return item->id;
}

bool cc_calvinsys_can_write(cc_calvinsys_t *calvinsys, char *id)
{
	cc_calvinsys_obj_t *obj = NULL;
	cc_list_t *item = NULL;

	item = cc_list_get(calvinsys->objects, id);
	if (item != NULL) {
		obj = (cc_calvinsys_obj_t *)item->data;
		if (obj->can_write != NULL)
			return obj->can_write(obj);
	}

	return false;
}

cc_result_t cc_calvinsys_write(cc_calvinsys_t *calvinsys, char *id, char *data, size_t data_size)
{
	cc_calvinsys_obj_t *obj = NULL;
	cc_list_t *item = NULL;

	item = cc_list_get(calvinsys->objects, id);
	if (item != NULL) {
		obj = (cc_calvinsys_obj_t *)item->data;
		if (obj->write != NULL)
			return obj->write(obj, data, data_size);
	}

	return CC_FAIL;
}

bool cc_calvinsys_can_read(cc_calvinsys_t *calvinsys, char *id)
{
	cc_calvinsys_obj_t *obj = NULL;
	cc_list_t *item = NULL;

	item = cc_list_get(calvinsys->objects, id);
	if (item != NULL) {
		obj = (cc_calvinsys_obj_t *)item->data;
		if (obj->can_read != NULL)
			return obj->can_read(obj);
	}

	return false;
}

cc_result_t cc_calvinsys_read(cc_calvinsys_t *calvinsys, char *id, char **data, size_t *data_size)
{
	cc_calvinsys_obj_t *obj = NULL;
	cc_list_t *item = NULL;

	item = cc_list_get(calvinsys->objects, id);
	if (item != NULL) {
		obj = (cc_calvinsys_obj_t *)item->data;
		if (obj->read != NULL)
			return obj->read(obj, data, data_size);
	}

	return CC_FAIL;
}

void cc_calvinsys_close(cc_calvinsys_t *calvinsys, char *id)
{
	cc_calvinsys_obj_t *obj = NULL;
	cc_list_t *item = NULL;

	item = cc_list_get(calvinsys->objects, id);
	if (item != NULL) {
		obj = (cc_calvinsys_obj_t *)item->data;
		if (obj->close != NULL)
			obj->close(obj);
		cc_log("calvinsys: Closed '%s'", id);
		cc_list_remove(&calvinsys->objects, id);
		cc_platform_mem_free((void *)obj);
	}
}

static uint32_t cc_calvinsys_get_number_of_attributes(cc_calvinsys_t *calvinsys, cc_actor_t *actor)
{
	cc_list_t *item = calvinsys->objects;
	uint32_t items = 0;
	cc_calvinsys_obj_t *obj = NULL;

	while (item != NULL) {
		obj = (cc_calvinsys_obj_t *)item->data;
		if (actor == obj->actor)
			items++;
		item = item->next;
	}

	return items;
}

cc_result_t cc_calvinsys_get_attributes(cc_calvinsys_t *calvinsys, cc_actor_t *actor, cc_list_t **private_attributes)
{
	cc_list_t *item = calvinsys->objects;
	cc_calvinsys_obj_t *obj = NULL;
	uint32_t nbr_of_items = cc_calvinsys_get_number_of_attributes(calvinsys, actor);
	char *buffer = NULL, *w = NULL;

	if (nbr_of_items == 0) {
		if (cc_platform_mem_alloc((void **)&buffer, 10) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}

		w = buffer;
		w = cc_coder_encode_map(w, 0);

		if (cc_list_add_n(private_attributes, "_calvinsys", 10, buffer, w - buffer) == NULL) {
			cc_log_error("Failed to add '_calvinsys'");
			cc_platform_mem_free((void *)buffer);
			return CC_FAIL;
		}

		return CC_SUCCESS;
	}

	// TODO: set a better size
	if (cc_platform_mem_alloc((void **)&buffer, nbr_of_items * 200) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	w = buffer;
	w = cc_coder_encode_map(w, nbr_of_items);
	while (item != NULL) {
		obj = (cc_calvinsys_obj_t *)item->data;
		if (actor == obj->actor) {
			w = cc_coder_encode_kv_map(w, item->id, 3);
			{
				w = cc_coder_encode_kv_str(w, "name", obj->capability->name, strlen(obj->capability->name));
				if (obj->serialize != NULL) {
					w = obj->serialize(item->id, obj, w);
					if (w == NULL) {
						cc_log_error("Failed to serialize object");
						cc_platform_mem_free((void *)buffer);
						return CC_FAIL;
					}
				} else
					w = cc_coder_encode_kv_map(w, "obj", 0);
				// TODO: Handle args
				w = cc_coder_encode_kv_map(w, "args", 0);
			}
		}
		item = item->next;
	}

	if (cc_list_add_n(private_attributes, "_calvinsys", 10, buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add '_calvinsys'");
		cc_platform_mem_free((void *)buffer);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_deserialize(struct cc_actor_t *actor, char *buffer)
{
	uint32_t index = 0, nbr_of_objects = cc_coder_decode_map(&buffer);
	uint32_t key_len = 0, name_len = 0;
	char *key = NULL, *name = NULL, *obj = NULL;
	cc_calvinsys_capability_t *capability = NULL;
	cc_calvinsys_obj_t *object = NULL;
	cc_list_t *item = NULL;

	for (index = 0; index < nbr_of_objects; index++) {
		if (cc_coder_decode_str(buffer, &key, &key_len) != CC_SUCCESS) {
			cc_log_error("Failed to decode key");
			return CC_FAIL;
		}

		cc_coder_decode_map_next(&buffer);

		if (cc_coder_decode_string_from_map(buffer, "name", &name, &name_len) != CC_SUCCESS) {
			cc_log_error("Failed to get 'name'");
			return CC_FAIL;
		}

		item = cc_list_get_n(actor->calvinsys->capabilities, name, name_len);
		if (item == NULL) {
			cc_log_error("Capability not registered");
			return CC_FAIL;
		}

		capability = (cc_calvinsys_capability_t *)item->data;

		if (cc_coder_get_value_from_map(buffer, "obj", &obj) != CC_SUCCESS) {
			cc_log_error("Failed to get 'obj'");
			return CC_FAIL;
		}

		if (cc_platform_mem_alloc((void **)&object, sizeof(cc_calvinsys_obj_t)) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}

		object->can_write = NULL;
		object->write = NULL;
		object->can_read = NULL;
		object->read = NULL;
		object->close = NULL;
		object->serialize = NULL;
		object->state = NULL;
		object->capability = capability;
		object->actor = actor;

		item = cc_list_add_n(&actor->calvinsys->objects, key, key_len, object, sizeof(cc_calvinsys_t));
		if (item == NULL) {
			cc_log_error("Failed to add '%s'", capability->name);
			cc_platform_mem_free((void *)object);
			return CC_FAIL;
		}
		object->id = item->id;

		if (capability->deserialize != NULL) {
			if (capability->deserialize(object, obj) != CC_SUCCESS) {
				cc_log_error("Failed to deserialize '%s'", capability->name);
				cc_platform_mem_free((void *)object);
				return CC_FAIL;
			}
		}

		cc_log("calvinsys: Deserialized '%s', capability '%s'", item->id, capability->name);
		cc_coder_decode_map_next(&buffer);
	}

	return CC_SUCCESS;
}
