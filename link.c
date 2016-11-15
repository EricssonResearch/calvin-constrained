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
#include "proto.h"

link_t *link_create(node_t *node, const char *peer_id, uint32_t peer_id_len, const link_state_t state)
{
	link_t *link = NULL;

	if (platform_mem_alloc((void **)&link, sizeof(link_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	memset(link, 0, sizeof(link_t));

	link->state = state;
	strncpy(link->peer_id, peer_id, peer_id_len);

	if (list_add(&node->links, link->peer_id, (void *)link, sizeof(link_t)) != SUCCESS) {
		log_error("Failed to add link");
		platform_mem_free((void *)link);
		return NULL;
	}

	log_debug("Link created to '%s'", link->peer_id);
	return link;
}

void link_free(node_t *node, link_t *link)
{
	log("Freeing link to '%s'", link->peer_id);
	list_remove(&node->links, link->peer_id);
	platform_mem_free((void *)link);
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

static result_t link_request_handler(char *data, void *msg_data)
{
	result_t result = FAIL;
	char *value = NULL, *peer_id = NULL, *value_data = NULL;
	uint32_t status = 0, peer_id_len = 0;
	node_t *node = node_get();
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
			log("Link to '%s' enabled", link->peer_id);
			link->state = LINK_ENABLED;
		} else {
			log_error("Link request failed");
			link->state = LINK_CONNECT_FAILED;
			result = FAIL;
		}
	}

	return result;
}

void link_remove(node_t *node, const char *peer_id)
{
	list_t *links = node->links;
	link_t *link = NULL;

	while (links != NULL) {
		link = (link_t *)links->data;
		if (strncmp(link->peer_id, peer_id, strlen(peer_id)) == 0) {
			link_free(node, link);
			return;
		}
		links = links->next;
	}
}

result_t link_transmit(node_t *node, link_t *link)
{
	switch (link->state) {
	case LINK_DO_CONNECT:
		link->state = LINK_PENDING;
		if (proto_send_route_request(node, link->peer_id, strlen(link->peer_id), link_request_handler) == SUCCESS)
			return SUCCESS;
		link->state = LINK_DO_CONNECT;
		break;
	default:
		break;
	}

	return FAIL;
}
