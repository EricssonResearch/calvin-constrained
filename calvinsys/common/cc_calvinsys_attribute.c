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
#include "cc_calvinsys_attribute.h"
#include "runtime/north/cc_common.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/coder/cc_coder.h"

typedef struct cc_calvinsys_attribute_t {
	char *attribute_name;
	char *attribute;
	uint32_t attribute_len;
} cc_calvinsys_attribute_t;

static bool cc_calvinsys_attribute_can_read(struct cc_calvinsys_obj_t *obj)
{
	cc_calvinsys_attribute_t *state = (cc_calvinsys_attribute_t *)obj->state;

  if (state->attribute != NULL || state->attribute_len != 0)
		return true;

	return false;
}

static cc_result_t cc_calvinsys_attribute_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	cc_calvinsys_attribute_t *state = (cc_calvinsys_attribute_t *)obj->state;

	if (state->attribute == NULL || state->attribute_len == 0) {
		cc_log_error("No attribute data");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)data, state->attribute_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memcpy(*data, state->attribute, state->attribute_len);
	*size = state->attribute_len;

	return CC_SUCCESS;
}

static bool cc_calvinsys_attribute_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t cc_calvinsys_attribute_get_attribute_value(char *attributes, char *attribute_name, uint32_t attribute_name_len, char **attribute, size_t *attribute_len)
{
	char *category = NULL, *item = NULL, *indexed_public = NULL;
	char *category_data = NULL;
	uint32_t pos = 0, category_len = 0, item_len = 0;
	size_t indexed_public_len = 0, category_data_len = 0;

	while (pos < attribute_name_len) {
		if (attribute_name[pos] == '.') {
			category = attribute_name;
			category_len = pos;
			item = attribute_name + (pos + 1);
			item_len = attribute_name_len - (pos + 1);
			break;
		}
		pos++;
	}

	if (category == NULL || item == NULL) {
		cc_log_error("Failed to parse attribute");
		return CC_FAIL;
	}

	if (cc_get_json_dict_value(
		attributes,
		strnlen(attributes, CC_MAX_ATTRIBUTES_LEN),
		(char *)"indexed_public",
		14,
		&indexed_public,
		&indexed_public_len) != CC_SUCCESS) {
			cc_log_error("Failed to get 'indexed_public'");
			return CC_FAIL;
	}

	if (cc_get_json_dict_value(
		indexed_public,
		indexed_public_len,
		category,
		category_len,
		&category_data,
		&category_data_len) != CC_SUCCESS) {
			cc_log_error("Failed to parse indexed public");
			return CC_FAIL;
	}

	return cc_get_json_string_value(category_data, category_data_len,	item,	item_len, attribute, attribute_len);
}

static cc_result_t cc_calvinsys_attribute_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	char *attributes = obj->capability->calvinsys->node->attributes;
	char *attribute_name = NULL, *attribute = NULL, *w = NULL;
	uint32_t attribute_name_len = 0;
	size_t attribute_len = 0;
	cc_calvinsys_attribute_t *state = (cc_calvinsys_attribute_t *)obj->state;

	if (state->attribute != NULL) {
		cc_platform_mem_free(state->attribute);
		state->attribute = NULL;
		state->attribute_len = 0;
	}

	if (state->attribute_name != NULL) {
		cc_platform_mem_free(state->attribute_name);
		state->attribute_name = NULL;
	}

	if (cc_coder_decode_str(data, &attribute_name, &attribute_name_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode attribute");
		return CC_FAIL;
	}

	if (cc_calvinsys_attribute_get_attribute_value(attributes, attribute_name, attribute_name_len, &attribute, &attribute_len) != CC_SUCCESS) {
		cc_log_error("Failed to get attribute");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state->attribute, cc_coder_sizeof_str(attribute_len)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	w = state->attribute;
	w = cc_coder_encode_str(w, attribute, attribute_len);
	if (w == NULL) {
		cc_log_error("Failed to encode attribute");
		return CC_FAIL;
	}
	state->attribute_len = w - state->attribute;

	if (cc_platform_mem_alloc((void **)&state->attribute_name, attribute_name_len + 1) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	strncpy(state->attribute_name, attribute_name, attribute_name_len);
	state->attribute_name[attribute_name_len] = '\0';

	return CC_SUCCESS;
}


static cc_result_t cc_calvinsys_attribute_close(struct cc_calvinsys_obj_t *obj)
{
	cc_calvinsys_attribute_t *state = (cc_calvinsys_attribute_t *)obj->state;

	if (state != NULL) {
		if (state->attribute_name != NULL)
			cc_platform_mem_free(state->attribute_name);
		if (state->attribute != NULL)
			cc_platform_mem_free(state->attribute);
		cc_platform_mem_free(state);
		state = NULL;
	}

	return CC_SUCCESS;
}

static char *cc_calvinsys_attribute_serialize(char *id, cc_calvinsys_obj_t *obj, char *buffer)
{
	cc_calvinsys_attribute_t *state = (cc_calvinsys_attribute_t *)obj->state;

	buffer = cc_coder_encode_kv_map(buffer, "obj", 2);
	{
		buffer = cc_coder_encode_kv_str(buffer, "type", "indexed", 7);
		buffer = cc_coder_encode_kv_str(buffer, "attribute", state->attribute_name, strlen(state->attribute_name));
	}

	return buffer;
}

static cc_result_t cc_calvinsys_attribute_open(cc_calvinsys_obj_t *obj, char *data, size_t len)
{
	cc_calvinsys_attribute_t *attribute = NULL;

	if (cc_platform_mem_alloc((void **)&attribute, sizeof(cc_calvinsys_attribute_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(attribute, 0, sizeof(cc_calvinsys_attribute_t));
	obj->can_write = cc_calvinsys_attribute_can_write;
	obj->write = cc_calvinsys_attribute_write;
	obj->can_read = cc_calvinsys_attribute_can_read;
	obj->read = cc_calvinsys_attribute_read;
	obj->close = cc_calvinsys_attribute_close;
	obj->serialize = cc_calvinsys_attribute_serialize;
	obj->state = attribute;

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_attribute_deserialize(cc_calvinsys_obj_t *obj, char *buffer)
{
	cc_calvinsys_attribute_t *state = NULL;
	char *attribute_name = NULL, *type = NULL, *attribute = NULL;
	uint32_t attribute_name_len = 0, type_len = 0;
	size_t attribute_len = 0;
	char *w = NULL, *attributes = obj->capability->calvinsys->node->attributes;

	if (cc_coder_decode_string_from_map(buffer, "type", &type, &type_len) != CC_SUCCESS) {
		cc_log_error("Failed to get attribute 'type'");
		return CC_FAIL;
	}

	if (strncmp(type, "indexed", 7) != 0) {
		cc_log_error("Only 'indexed' attributes supported");
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(buffer, "attribute", &attribute_name, &attribute_name_len) != CC_SUCCESS) {
		cc_log_error("Failed to get attribute 'attribute'");
		return CC_FAIL;
	}

	if (cc_calvinsys_attribute_get_attribute_value(attributes, attribute_name, attribute_name_len, &attribute, &attribute_len) != CC_SUCCESS) {
		cc_log_error("Failed to get attribute");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_calvinsys_attribute_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state->attribute, cc_coder_sizeof_str(attribute_len)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	w = state->attribute;
	w = cc_coder_encode_str(w, attribute, attribute_len);
	if (w == NULL) {
		cc_log_error("Failed to encode attribute");
		cc_platform_mem_free(state->attribute);
		cc_platform_mem_free(state);
		return CC_FAIL;
	}
	state->attribute_len = w - state->attribute;

	if (cc_platform_mem_alloc((void **)&state->attribute_name, attribute_name_len + 1) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		cc_platform_mem_free(state->attribute);
		cc_platform_mem_free(state);
		return CC_FAIL;
	}
	strncpy(state->attribute_name, attribute_name, attribute_name_len);
	state->attribute_name[attribute_name_len] = '\0';

	obj->can_write = cc_calvinsys_attribute_can_write;
	obj->write = cc_calvinsys_attribute_write;
	obj->can_read = cc_calvinsys_attribute_can_read;
	obj->read = cc_calvinsys_attribute_read;
	obj->close = cc_calvinsys_attribute_close;
	obj->serialize = cc_calvinsys_attribute_serialize;
	obj->state = state;

	cc_log_debug("Attribute deserialized, name '%s'", state->attribute_name);

	return CC_SUCCESS;
}

cc_result_t cc_calvinsys_attribute_create(cc_calvinsys_t **calvinsys)
{
	return cc_calvinsys_create_capability(*calvinsys, "sys.attribute.indexed", cc_calvinsys_attribute_open, cc_calvinsys_attribute_deserialize, NULL);
}
