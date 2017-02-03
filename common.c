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
#include "common.h"
#include "platform.h"

// TODO: Generate a proper uuid
void gen_uuid(char *buffer, const char *prefix)
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

bool uuid_is_higher(char *id1, size_t len1, char *id2, size_t len2)
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

result_t list_add_n(list_t **head, char *id, uint32_t len, void *data, uint32_t data_len)
{
	char *name = NULL;

	if (platform_mem_alloc((void **)&name, sizeof(char) * (len + 1)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	strncpy(name, id, len);
	name[len] = '\0';

	if (list_add(head, name, data, data_len) == FAIL) {
		platform_mem_free(name);
		return FAIL;
	}

	return SUCCESS;
}

result_t list_add(list_t **head, char *id, void *data, uint32_t data_len)
{
	list_t *new_item = NULL, *tmp = NULL;

	if (platform_mem_alloc((void **)&new_item, sizeof(list_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	new_item->id = id;
	new_item->data = data;
	new_item->data_len = data_len;
	new_item->next = NULL;

	if (*head == NULL) {
		*head = new_item;
	} else {
		tmp = *head;
		while (tmp->next != NULL)
			tmp = tmp->next;
		tmp->next = new_item;
	}

	return SUCCESS;
}

void list_remove(list_t **head, const char *id)
{
	list_t *curr = NULL, *prev = NULL;

	for (curr = *head; curr != NULL; prev = curr, curr = curr->next) {
		if (strncmp(curr->id, id, strlen(curr->id)) == 0) {
			if (prev == NULL)
				*head = curr->next;
			else
				prev->next = curr->next;
			platform_mem_free((void *)curr);
			return;
		}
	}
}

uint32_t list_count(list_t *list)
{
	uint32_t count = 0;
	list_t *tmp = list;

	while (tmp != NULL) {
		count++;
		tmp = tmp->next;
	}

	return count;
}

void *list_get_n(list_t *list, const char *id, uint32_t id_len)
{
	list_t *tmp = list;

	while (tmp != NULL) {
		if (strncmp(tmp->id, id, id_len) == 0)
			return (void *)tmp->data;
		tmp = tmp->next;
	}

	return NULL;
}

void *list_get(list_t *list, const char *id)
{
	list_t *tmp = list;

	while (tmp != NULL) {
		if (strncmp(tmp->id, id, strlen(tmp->id)) == 0)
			return (void *)tmp->data;
		tmp = tmp->next;
	}

	return NULL;
}
