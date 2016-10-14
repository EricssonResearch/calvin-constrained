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
#include <string.h>
#include "common.h"

// TODO: Generate a proper uuid
char *gen_uuid(const char *prefix)
{
	char *str = NULL;
	int i, len_prefix = 0;
	const char *hex_digits = "0123456789abcdef";

	if (prefix != NULL)
		len_prefix = strlen(prefix);

	str = (char *)malloc((37 + len_prefix) * sizeof(char));
	if (str == NULL)
		return NULL;

	for (i = 0; i < len_prefix; i++)
		str[i] = prefix[i];

	for (i = 0; i < 36; i++)
		str[i + len_prefix] = hex_digits[(rand() % 16)];

	str[8 + len_prefix] = str[13 + len_prefix] = str[18 + len_prefix] = str[23 + len_prefix] = '-';
	str[36 + len_prefix] = '\0';

	return str;
}

char *get_highest_uuid(char* id1, char* id2)
{
	size_t len1 = 0, len2 = 0;
	int i = 0;

	len1 = strlen(id1);
	len2 = strlen(id2);

	if (len1 > len2)
		return id1;

	if (len2 > len1)
		return id2;

	for (i = len1; i >= 0; i--) {
		if (id1[i] > id2[i])
			return id1;

		if (id2[i] > id1[i])
			return id2;
	}

	return id1;
}

unsigned int get_message_len(const char *buffer)
{
	unsigned int value =
		((buffer[3] & 0xFF) <<  0) |
		((buffer[2] & 0xFF) <<  8) |
		((buffer[1] & 0xFF) << 16) |
		((buffer[0] & 0xFF) << 24);
	return value;
}
