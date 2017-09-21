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
#include "../../runtime/north/cc_common.h"
#include "../../runtime/north/cc_node.h"
#include "../../runtime/north/coder/cc_coder.h"

typedef struct cc_calvinsys_attribute_t {
	char *attribute;
	size_t len;
} cc_calvinsys_attribute_t;

static bool calvinsys_attribute_can_read(struct cc_calvinsys_obj_t *obj)
{
	cc_calvinsys_attribute_t *state = (cc_calvinsys_attribute_t *)obj->state;

  if (state->attribute != NULL && state->len > 0)
		return true;

	return false;
}

static cc_result_t calvinsys_attribute_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	cc_calvinsys_attribute_t *state = (cc_calvinsys_attribute_t *)obj->state;
	char *tmp = NULL;

	if (state->attribute != NULL && state->len > 0) {
		if (cc_platform_mem_alloc((void **)data, state->len + 1) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}

		tmp = cc_coder_encode_str(*data, state->attribute, state->len);
		if (tmp == NULL) {
			cc_log_error("Failed to encode attribute");
			return CC_FAIL;
		}

		*size = tmp - *data;

		return CC_SUCCESS;
	}

	return CC_FAIL;
}

static bool calvinsys_attribute_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t calvinsys_attribute_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	char *attributes = obj->handler->calvinsys->node->attributes;
	char *attribute = NULL, *indexed_public = NULL, *category = NULL, *category_data = NULL, *item = NULL, *value = NULL;
	size_t pos = 0, indexed_public_len = 0, category_len = 0, category_data_len = 0, item_len = 0, value_len = 0;
	uint32_t attribute_len = 0;
	cc_calvinsys_attribute_t *state = (cc_calvinsys_attribute_t *)obj->state;

	if (state->attribute != NULL) {
		cc_platform_mem_free((void *)state->attribute);
		state->attribute = NULL;
		state->len = 0;
	}

	if (cc_coder_decode_str(data, &attribute, &attribute_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode attribute");
		return CC_FAIL;
	}

	while (pos < attribute_len) {
		if (attribute[pos] == '.') {
			category = attribute;
			category_len = pos;

			item = attribute + (pos + 1);
			item_len = attribute_len - (pos + 1);

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
			cc_log_error("Failed to get '%.*s' from '%.*s'", category_len, category, indexed_public_len, indexed_public);
			return CC_FAIL;
	}

	if (cc_get_json_string_value(
		category_data,
		category_data_len,
		item,
		item_len,
		&value,
		&value_len) != CC_SUCCESS) {
			cc_log_error("Failed to get '%.*s'", category_data_len, category_data);
			return CC_FAIL;
	}

	state->attribute = value;
	state->len = value_len;

	return CC_SUCCESS;
}


static cc_result_t calvinsys_attribute_close(struct cc_calvinsys_obj_t *obj)
{
	cc_calvinsys_attribute_t *state = (cc_calvinsys_attribute_t *)obj->state;

	if (state != NULL) {
		cc_platform_mem_free((void *)state);
		state = NULL;
	}

	return CC_SUCCESS;
}

static cc_calvinsys_obj_t *calvinsys_attribute_open(cc_calvinsys_handler_t *handler, char *data, size_t len, void *state, uint32_t id, const char *capability_name)
{
	cc_calvinsys_obj_t *obj = NULL;
	cc_calvinsys_attribute_t *attribute = NULL;

	if (cc_platform_mem_alloc((void **)&attribute, sizeof(cc_calvinsys_attribute_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	attribute->attribute = NULL;

	if (cc_platform_mem_alloc((void **)&obj, sizeof(cc_calvinsys_obj_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		cc_platform_mem_free((void *)attribute);
		return NULL;
	}

	obj->can_write = calvinsys_attribute_can_write;
	obj->write = calvinsys_attribute_write;
	obj->can_read = calvinsys_attribute_can_read;
	obj->read = calvinsys_attribute_read;
	obj->close = calvinsys_attribute_close;
	obj->state = attribute;

	return obj;
}

cc_result_t cc_calvinsys_attribute_create(cc_calvinsys_t **calvinsys)
{
	cc_calvinsys_handler_t *handler = NULL;

	if (cc_platform_mem_alloc((void **)&handler, sizeof(cc_calvinsys_handler_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	handler->open = calvinsys_attribute_open;
	handler->objects = NULL;
	handler->next = NULL;

	cc_calvinsys_add_handler(calvinsys, handler);
	if (cc_calvinsys_register_capability(*calvinsys, "sys.attribute.indexed", handler, NULL) != CC_SUCCESS)
		return CC_FAIL;

	return CC_SUCCESS;
}
