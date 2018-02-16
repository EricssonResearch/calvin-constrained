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
#ifndef CC_COMMON_H
#define CC_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "jsmn/jsmn.h"

#define CC_UUID_BUFFER_SIZE				50
#define CC_ATTRIBUTE_BUFFER_SIZE	1024
#define CC_MAX_URI_LEN						50
#define CC_MAX_ATTRIBUTES_LEN			1024

// common return codes
typedef enum {
	CC_SUCCESS,
	CC_FAIL,
	CC_PENDING
} cc_result_t;

// list item with a string identifier
typedef struct cc_list_t {
	char *id;
	uint32_t id_len;
	void *data;
	uint32_t data_len;
	bool free_id;
	struct cc_list_t *next;
} cc_list_t;

/**
 * cc_gen_uuid() - Fill buffer with a 36 byte UUID and optional prefix
 * @buffer Buffer to fill
 * @prefix Optional prefix
 */
void cc_gen_uuid(char *buffer, const char *prefix);

/**
 * cc_uuid_is_higher() - Compare uuids
 * @id1 First uuid
 * @len1 Size of first uui1
 * @id2 Second uuid
 * @len2 Size of seconds uuid
 *
 * Return: true if id1 is higher than id2 otherwise false
 */
bool cc_uuid_is_higher(char *id1, size_t len1, char *id2, size_t len2);

/**
 * cc_list_add() - Add item to list
 * @head List head
 * @id Terminated ID of item to add
 * @data Data to add
 * @data_len Size of data
 *
 * Return: Pointer to new list item of NULL on error
 */
cc_list_t *cc_list_add(cc_list_t **head, char *id, void *data, uint32_t data_len);

/**
 * cc_list_add_n() - Add item to list
 * @head List head
 * @id ID of item to add
 * @len Length of id
 * @data Data to add
 * @data_len Size of data
 *
 * Return: Pointer to new list item of NULL on error
 */
cc_list_t *cc_list_add_n(cc_list_t **head, const char *id, uint32_t len, void *data, uint32_t data_len);

/**
 * cc_list_remove() - Remove item with id from list
 * @head List head
 * @id ID of item to remove
 */
void cc_list_remove(cc_list_t **head, const char *id);

/**
 * cc_list_count() - Get number of list elements
 * @list List head
 *
 * Return: Number of elements
 */
uint32_t cc_list_count(cc_list_t *list);

/**
 * cc_list_get_n() - Get item from list
 * @list List head
 * @list ID of item
 * @list Length of ID
 *
 * Return: Pointer to list item or NULL
 */
cc_list_t *cc_list_get_n(cc_list_t *list, const char *id, uint32_t id_len);

/**
 * cc_list_get() - Get item from list
 * @list List head
 * @list Teminated ID of item
 *
 * Return: Pointer to list item or NULL
 */
cc_list_t *cc_list_get(cc_list_t *list, const char *id);

/**
 * cc_json_get_dict_value() - Get value with JSON object
 * @buffer The JSON buffer
 * @object The JSON object to iterate
 * @count Number of tokens
 * @key The key to search for
 * @key_len Length of key
 *
 * Return: Pointer to token value or NULL if not found
 */
jsmntok_t *cc_json_get_dict_value(const char *buffer, jsmntok_t *object, size_t count, char *key, size_t key_len);

#endif /* CC_COMMON_H */
