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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cc_common.h"
#include "../south/platform/cc_platform.h"

// TODO: Generate a proper uuid
void cc_gen_uuid(char *buffer, const char *prefix)
{
	int i, len_prefix = 0;
	const char *hex_digits = "0123456789abcdef";

	if (prefix != NULL)
		len_prefix = strlen(prefix);

	for (i = 0; i < len_prefix; i++)
		buffer[i] = prefix[i];

	for (i = 0; i < 36; i++)
		buffer[i + len_prefix] = hex_digits[(rand() % 16)];

	buffer[8 + len_prefix] = buffer[13 + len_prefix] = buffer[18 + len_prefix] = buffer[23 + len_prefix] = '-';
	buffer[36 + len_prefix] = '\0';
}

bool cc_uuid_is_higher(char *id1, size_t len1, char *id2, size_t len2)
{
	int i = 0;

	if (len1 > len2)
		return true;

	if (len2 > len1)
		return false;

	for (i = len1; i >= 0; i--) {
		if (id1[i] > id2[i])
			return true;
	}

	return false;
}

static cc_result_t cc__list_add(cc_list_t **head, char *id, bool free_id, void *data, uint32_t data_len)
{
	cc_list_t *new_item = NULL, *tmp = NULL;

	if (cc_platform_mem_alloc((void **)&new_item, sizeof(cc_list_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	new_item->id = id;
	new_item->id_len = strlen(id);
	new_item->data = data;
	new_item->data_len = data_len;
	new_item->next = NULL;
	new_item->free_id = free_id;

	if (*head == NULL) {
		*head = new_item;
	} else {
		tmp = *head;
		while (tmp->next != NULL)
			tmp = tmp->next;
		tmp->next = new_item;
	}

	return CC_SUCCESS;
}

cc_result_t cc_list_add_n(cc_list_t **head, const char *id, uint32_t len, void *data, uint32_t data_len)
{
	char *name = NULL;

	if (cc_platform_mem_alloc((void **)&name, sizeof(char) * (len + 1)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	strncpy(name, id, len);
	name[len] = '\0';

	if (cc__list_add(head, name, true, data, data_len) == CC_FAIL) {
		cc_platform_mem_free(name);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_list_add(cc_list_t **head, char *id, void *data, uint32_t data_len)
{
	return cc__list_add(head, id, false, data, data_len);
}

static void cc_list_free_item(cc_list_t *item)
{
	if (item->free_id)
		cc_platform_mem_free((void *)item->id);
	cc_platform_mem_free((void *)item);
}

void cc_list_remove(cc_list_t **head, const char *id)
{
	cc_list_t *curr = NULL, *prev = NULL;

	for (curr = *head; curr != NULL; prev = curr, curr = curr->next) {
		if (strncmp(curr->id, id, strlen(curr->id)) == 0) {
			if (prev == NULL)
				*head = curr->next;
			else
				prev->next = curr->next;
			cc_list_free_item(curr);
			return;
		}
	}
}

uint32_t cc_list_count(cc_list_t *list)
{
	uint32_t count = 0;
	cc_list_t *tmp = list;

	while (tmp != NULL) {
		count++;
		tmp = tmp->next;
	}

	return count;
}

void *cc_list_get_n(cc_list_t *list, const char *id, uint32_t id_len)
{
	cc_list_t *tmp = list;

	while (tmp != NULL) {
		if (strncmp(tmp->id, id, id_len) == 0)
			return (void *)tmp->data;
		tmp = tmp->next;
	}

	return NULL;
}

void *cc_list_get(cc_list_t *list, const char *id)
{
	cc_list_t *tmp = list;

	while (tmp != NULL) {
		if (strncmp(tmp->id, id, strlen(tmp->id)) == 0)
			return (void *)tmp->data;
		tmp = tmp->next;
	}

	return NULL;
}

cc_result_t cc_get_json_string_value(char *buffer, size_t buffer_len, char *key, size_t key_len, char **value, size_t *value_len)
{
	char *start = NULL;
	size_t pos = 0;

	while ((pos + key_len) < buffer_len) {
		if (strncmp(buffer + pos, key, key_len) == 0) {
			start = buffer + pos;
			break;
		}
		pos++;
	}

	if (start != NULL) {
		start = start + key_len + 1;
		pos = start - buffer;
		start = NULL;
		while (pos < buffer_len) {
			if (buffer[pos] == '\"') {
				if (start == NULL) {
					start = buffer + pos + 1;
				} else {
					*value = start;
					*value_len = (buffer + pos) - start;
					return CC_SUCCESS;
				}
			}
			pos++;
		}
	}

	return CC_FAIL;
}

cc_result_t cc_get_json_dict_value(char *buffer, size_t buffer_len, char *key, size_t key_len, char **value, size_t *value_len)
{
	char *start = NULL;
	size_t pos = 0, braces = 0, len = 0;

	while ((pos + key_len) < buffer_len) {
		if (strncmp(buffer + pos, key, key_len) == 0) {
			start = buffer + pos;
			break;
		}
		pos++;
	}

	if (start != NULL) {
		start = start + key_len + 1;
		pos = start - buffer;
		start = NULL;
		while (pos < buffer_len) {
			if (buffer[pos] == '{') {
				if (start == NULL) {
					start = buffer + pos;
				}
				braces++;
			}

			if (buffer[pos] == '}') {
				braces--;
			}

			if (start != NULL) {
				len++;
				if (braces == 0) {
					*value_len = len;
					*value = start;
					return CC_SUCCESS;
				}
			}
			pos++;
		}
	} else
		cc_log_error("Failed to get '%.*s' from '%.*s'", key_len, key, buffer_len, buffer);

	return CC_FAIL;
}

#ifdef CC_ADD_STRNSTR
char* strnstr(const char* buffer, const char* token, size_t n)
{
	const char* p;
	int len = (int)strlen(token);

	if (len == 0) {
		return (char *)buffer;
	}

	for (p = buffer; *p && (p + len <= buffer + n); p++) {
		if ((*p == *token) && (strncmp(p, token, len) == 0)) {
			return (char *)p;
		}
	}
	return NULL;
}
#endif
