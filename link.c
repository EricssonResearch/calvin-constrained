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

link_t *create_link(char *peer_id, link_state_t state)
{
	link_t *link = NULL;

	link = (link_t *)malloc(sizeof(link_t));
	if (link == NULL) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	link->peer_id = strdup(peer_id);
	link->state = state;

	log_debug("Created link to '%s'", link->peer_id);

	return link;
}

result_t add_link(node_t *node, link_t *link)
{
	int i_link = 0;

	for (i_link = 0; i_link < MAX_LINKS; i_link++) {
		if (node->links[i_link] == NULL) {
			node->links[i_link] = link;
			return SUCCESS;
		}
	}

	log_error("Failed to add link");

	return FAIL;
}

link_t *get_link(node_t *node, char *peer_id)
{
	int i_link = 0;

	if (strcmp(node->proxy_link->peer_id, peer_id) == 0)
		return node->proxy_link;

	for (i_link = 0; i_link < MAX_LINKS; i_link++) {
		if (node->links[i_link] != NULL && strcmp(peer_id, node->links[i_link]->peer_id) == 0)
			return node->links[i_link];
	}

	return NULL;
}

static result_t request_link_handler(char *data, void *msg_data)
{
	result_t result = FAIL;
	char *value = NULL, *peer_id = NULL, *value_data = NULL;
	uint32_t status;
	node_t *node = get_node();
	link_t *link = NULL;
	tunnel_t *tunnel = NULL;

	result = get_value_from_map(&data, "value", &value);

	if (result == SUCCESS)
		result = decode_uint_from_map(&value, "status", &status);

	if (result == SUCCESS)
		result = get_value_from_map(&value, "data", &value_data);

	if (result == SUCCESS)
		result = decode_string_from_map(&value_data, "peer_id", &peer_id);

	if (result != SUCCESS)
		log_error("Failed to decode message");

	if (result == SUCCESS) {
		if (status != 200) {
			log_error("Link request failed with status %ld", (unsigned long)status);
			result = FAIL;
		}
	}

	link = create_link(peer_id, LINK_WORKING);
	if (link != NULL) {
		tunnel = get_tunnel_from_peerid(node, link->peer_id);
		if (tunnel == NULL) {
			log_error("Link requested without a tunnel");
			result = FAIL;
			free_link(link);
		}

		result = add_link(node, link);
		if (result != SUCCESS) {
			log_error("Failed to add link");
			free_link(link);
		}

		if (result == SUCCESS)
			result = request_token_tunnel(node, tunnel);
	} else {
		log_error("Failed to create link");
		result = FAIL;
	}


	if (peer_id != NULL)
		free(peer_id);

	return result;
}

result_t request_link(node_t *node, link_t *link)
{
	result_t result = FAIL;

	if (link != NULL)
		result = send_route_request(node, link->peer_id, request_link_handler);

	return result;
}

void remove_link(node_t *node, link_t *link)
{
	int i_link = 0;

	log_debug("Removing link");
	for (i_link = 0; i_link < MAX_LINKS; i_link++) {
		if (node->links[i_link] != NULL && strcmp(link->peer_id, node->links[i_link]->peer_id) == 0)
			node->links[i_link] = NULL;
	}
}

void free_link(link_t *link)
{
	log_debug("Freeing link");
	if (link != NULL) {
		if (link->peer_id != NULL)
			free(link->peer_id);
		free(link);
	}
}
