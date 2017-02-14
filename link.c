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
#include "link.h"
#include "node.h"
#include "platform.h"
#include "msgpack_helper.h"
#include "msgpuck/msgpuck.h"
#include "proto.h"

link_t *link_create(node_t *node, const char *peer_id, uint32_t peer_id_len, const link_state_t state, bool is_proxy)
{
	link_t *link = NULL;

	link = link_get(node, peer_id, peer_id_len);
	if (link != NULL)
		return link;

	if (platform_mem_alloc((void **)&link, sizeof(link_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	memset(link, 0, sizeof(link_t));

	link->is_proxy = is_proxy;
	link->state = state;
	strncpy(link->peer_id, peer_id, peer_id_len);
	link->ref_count = 0;

	if (list_add(&node->links, link->peer_id, (void *)link, sizeof(link_t)) != SUCCESS) {
		log_error("Failed to add link");
		platform_mem_free((void *)link);
		return NULL;
	}

	log("Link created, peer id '%s' is_proxy '%s'", link->peer_id, link->is_proxy ? "yes" : "no");

	return link;
}

char *link_serialize(const link_t *link, char **buffer)
{
	*buffer = mp_encode_map(*buffer, 3);
	{
		*buffer = encode_str(buffer, "peer_id", link->peer_id, strlen(link->peer_id));
		*buffer = encode_uint(buffer, "state", link->state);
		*buffer = encode_bool(buffer, "is_proxy", link->is_proxy);
	}

	return *buffer;
}

link_t *link_deserialize(node_t *node, char *buffer)
{
	char *peer_id = NULL;
	uint32_t len = 0, state = 0;
	bool is_proxy = false;

	if (decode_string_from_map(buffer, "peer_id", &peer_id, &len) != SUCCESS)
		return NULL;

	if (decode_uint_from_map(buffer, "state", &state) != SUCCESS)
		return NULL;

	if (decode_bool_from_map(buffer, "is_proxy", &is_proxy) != SUCCESS)
		return NULL;

	return link_create(node, peer_id, len, (const link_state_t)state, is_proxy);
}

void link_free(node_t *node, link_t *link)
{
	log("Deleting link to '%s'", link->peer_id);
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

static result_t link_request_handler(node_t *node, char *data, void *msg_data)
{
	result_t result = FAIL;
	char *value = NULL, *peer_id = NULL, *value_data = NULL;
	uint32_t status = 0, peer_id_len = 0;
	link_t *link = NULL;

	result = get_value_from_map(data, "value", &value);

	if (result == SUCCESS)
		result = decode_uint_from_map(value, "status", &status);

	if (result == SUCCESS)
		result = get_value_from_map(value, "data", &value_data);

	if (result == SUCCESS)
		result = decode_string_from_map(value_data, "peer_id", &peer_id, &peer_id_len);

	if (result != SUCCESS) {
		log_error("Failed to decode message");
		return FAIL;
	}

	link = link_get(node, peer_id, peer_id_len);
	if (link == NULL) {
		log_error("No link to '%.*s'", (int)peer_id_len, peer_id);
		return FAIL;
	}

	if (result == SUCCESS) {
		if (status == 200) {
			log_debug("Link to '%s' enabled", link->peer_id);
			link->state = LINK_ENABLED;
		} else {
			log_error("Link request failed");
			link->state = LINK_CONNECT_FAILED;
			result = FAIL;
		}
	}

	return result;
}

void link_transmit(node_t *node, link_t *link)
{
	switch (link->state) {
	case LINK_DO_CONNECT:
		if (proto_send_route_request(node, link->peer_id, strlen(link->peer_id), link_request_handler) == SUCCESS)
			link->state = LINK_PENDING;
		break;
	default:
		break;
	}
}
