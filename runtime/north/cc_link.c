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
#include "cc_link.h"
#include "cc_node.h"
#include "cc_proto.h"
#include "cc_msgpack_helper.h"
#include "../../msgpuck/msgpuck.h"
#include "../south/platform/cc_platform.h"

link_t *link_create(node_t *node, const char *peer_id, uint32_t peer_id_len, bool is_proxy)
{
	link_t *link = NULL;

	link = link_get(node, peer_id, peer_id_len);
	if (link != NULL)
		return link;

	if (platform_mem_alloc((void **)&link, sizeof(link_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	memset(link, 0, sizeof(link_t));

	link->is_proxy = is_proxy;
	strncpy(link->peer_id, peer_id, peer_id_len);
	link->ref_count = 0;

	if (list_add(&node->links, link->peer_id, (void *)link, sizeof(link_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to add link");
		platform_mem_free((void *)link);
		return NULL;
	}

	log_debug("Link created, peer id '%s' is_proxy '%s'", link->peer_id, link->is_proxy ? "yes" : "no");

	return link;
}

char *link_serialize(const link_t *link, char **buffer)
{
	*buffer = mp_encode_map(*buffer, 2);
	{
		*buffer = encode_str(buffer, "peer_id", link->peer_id, strlen(link->peer_id));
		*buffer = encode_bool(buffer, "is_proxy", link->is_proxy);
	}

	return *buffer;
}

link_t *link_deserialize(node_t *node, char *buffer)
{
	char *peer_id = NULL;
	uint32_t len = 0;
	bool is_proxy = false;

	if (decode_string_from_map(buffer, "peer_id", &peer_id, &len) != CC_RESULT_SUCCESS)
		return NULL;

	if (decode_bool_from_map(buffer, "is_proxy", &is_proxy) != CC_RESULT_SUCCESS)
		return NULL;

	return link_create(node, peer_id, len, is_proxy);
}

void link_free(node_t *node, link_t *link)
{
	log_debug("Deleting link to '%s'", link->peer_id);
	list_remove(&node->links, link->peer_id);
	platform_mem_free((void *)link);
}

void link_add_ref(link_t *link)
{
	if (link != NULL) {
		link->ref_count++;
		log_debug("Link ref added '%s' ref: %d", link->peer_id, link->ref_count);
	}
}

void link_remove_ref(node_t *node, link_t *link)
{
	if (link != NULL) {
		link->ref_count--;
		log_debug("Link ref removed '%s' ref: %d", link->peer_id, link->ref_count);
		if (link->ref_count == 0)
			link_free(node, link);
	}
}

link_t *link_get(node_t *node, const char *peer_id, uint32_t peer_id_len)
{
	list_t *links = node->links;
	link_t *link = NULL;

	while (links != NULL) {
		link = (link_t *)links->data;
		if (strncmp(link->peer_id, peer_id, peer_id_len) == 0)
			return link;
		links = links->next;
	}

	return NULL;
}
