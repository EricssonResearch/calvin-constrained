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
#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_TOKENS				5
#define MAX_PENDING_MSGS	10
#define MAX_TOKEN_SIZE		10
#define STORAGE_TUNNEL		"storage"
#define TOKEN_TUNNEL			"token"
#define UUID_BUFFER_SIZE	50
#define SERIALIZER				"msgpack"

typedef enum {
	SUCCESS,
	FAIL,
	PENDING
} result_t;

// list with a string identifier
typedef struct list_t {
	char *id;
	void *data;
	uint32_t data_len;
	struct list_t *next;
} list_t;

void gen_uuid(char *buffer, const char *prefix);
bool uuid_is_higher(char *id1, size_t len1, char *id2, size_t len2);
result_t list_add(list_t **head, char *id, void *data, uint32_t data_len);
result_t list_addn(list_t **head, char *id, uint32_t len, void *data, uint32_t data_len);
void list_remove(list_t **head, const char *id);
uint32_t list_count(list_t *list);
void *list_get_n(list_t *list, const char *id, uint32_t id_len);
void *list_get(list_t *list, const char *id);

#endif /* COMMON_H */
