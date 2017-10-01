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
#include "coder/cc_coder.h"
#include "../south/platform/cc_platform.h"

cc_link_t *cc_link_create(cc_node_t *node, const char *peer_id, uint32_t peer_id_len, bool is_proxy)
{
	cc_link_t *link = NULL;

	link = cc_link_get(node, peer_id, peer_id_len);
	if (link != NULL)
		return link;

	if (cc_platform_mem_alloc((void **)&link, sizeof(cc_link_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	memset(link, 0, sizeof(cc_link_t));

	link->is_proxy = is_proxy;
	strncpy(link->peer_id, peer_id, peer_id_len);
	link->ref_count = 0;

	if (cc_list_add(&node->links, link->peer_id, (void *)link, sizeof(cc_link_t)) == NULL) {
		cc_log_error("Failed to add link");
		cc_platform_mem_free((void *)link);
		return NULL;
	}

	cc_log_debug("Link created, peer id '%s' is_proxy '%s'", link->peer_id, link->is_proxy ? "yes" : "no");

	return link;
}

char *cc_link_serialize(const cc_link_t *link, char *buffer)
{
	buffer = cc_coder_encode_map(buffer, 2);
	{
		buffer = cc_coder_encode_kv_str(buffer, "peer_id", link->peer_id, strnlen(link->peer_id, CC_UUID_BUFFER_SIZE));
		buffer = cc_coder_encode_kv_bool(buffer, "is_proxy", link->is_proxy);
	}

	return buffer;
}

cc_link_t *cc_link_deserialize(cc_node_t *node, char *buffer)
{
	char *peer_id = NULL;
	uint32_t len = 0;
	bool is_proxy = false;

	if (cc_coder_decode_string_from_map(buffer, "peer_id", &peer_id, &len) != CC_SUCCESS)
		return NULL;

	if (cc_coder_decode_bool_from_map(buffer, "is_proxy", &is_proxy) != CC_SUCCESS)
		return NULL;

	return cc_link_create(node, peer_id, len, is_proxy);
}

void cc_link_free(cc_node_t *node, cc_link_t *link)
{
	cc_log_debug("Deleting link to '%s'", link->peer_id);
	cc_list_remove(&node->links, link->peer_id);
	cc_platform_mem_free((void *)link);
}

void cc_link_add_ref(cc_link_t *link)
{
	if (link != NULL) {
		link->ref_count++;
		cc_log_debug("Link ref added '%s' ref: %d", link->peer_id, link->ref_count);
	}
}

void cc_link_remove_ref(cc_node_t *node, cc_link_t *link)
{
	if (link != NULL) {
		link->ref_count--;
		cc_log_debug("Link ref removed '%s' ref: %d", link->peer_id, link->ref_count);
		if (link->ref_count == 0)
			cc_link_free(node, link);
	}
}

cc_link_t *cc_link_get(cc_node_t *node, const char *peer_id, uint32_t peer_id_len)
{
	cc_list_t *links = node->links;
	cc_link_t *link = NULL;

	while (links != NULL) {
		link = (cc_link_t *)links->data;
		if (strncmp(link->peer_id, peer_id, peer_id_len) == 0)
			return link;
		links = links->next;
	}

	return NULL;
}
