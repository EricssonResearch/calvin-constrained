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

#define CC_STORAGE_TUNNEL					"storage"
#define CC_TOKEN_TUNNEL						"token"
#define CC_UUID_BUFFER_SIZE				50
#define CC_ATTRIBUTE_BUFFER_SIZE	1024
#define CC_MAX_URI_LEN						50
#define CC_MAX_ATTRIBUTES_LEN			1024
#define CC_MAX_DIR_PATH						100
#define CC_CONFIG_FILE						"calvin.msgpack"

typedef enum {
	CC_SUCCESS,
	CC_FAIL,
	CC_PENDING
} cc_result_t;

// list with a string identifier
typedef struct cc_list_t {
	char *id;
	uint32_t id_len;
	void *data;
	uint32_t data_len;
	bool free_id;
	struct cc_list_t *next;
} cc_list_t;

void cc_gen_uuid(char *buffer, const char *prefix);
bool cc_uuid_is_higher(char *id1, size_t len1, char *id2, size_t len2);
cc_list_t *cc_list_add(cc_list_t **head, char *id, void *data, uint32_t data_len);
cc_list_t *cc_list_add_n(cc_list_t **head, const char *id, uint32_t len, void *data, uint32_t data_len);
void cc_list_remove(cc_list_t **head, const char *id);
uint32_t cc_list_count(cc_list_t *list);
cc_list_t *cc_list_get_n(cc_list_t *list, const char *id, uint32_t id_len);
cc_list_t *cc_list_get(cc_list_t *list, const char *id);
cc_result_t cc_get_json_string_value(char *buffer, size_t buffer_len, char *key, size_t key_len, char **value, size_t *value_len);
cc_result_t cc_get_json_dict_value(char *buffer, size_t buffer_len, char *key, size_t key_len, char **value, size_t *value_len);
#ifdef CC_ADD_STRNSTR
char* strnstr(const char* buffer, const char* token, size_t n);
#endif

#endif /* CC_COMMON_H */
