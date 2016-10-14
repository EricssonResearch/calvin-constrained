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
#include "tunnel.h"
#include "platform.h"
#include "proto.h"
#include "msgpack_helper.h"
#include "node.h"
#include "link.h"


tunnel_t *create_tunnel_from_id(link_t *link, tunnel_type_t type, tunnel_state_t state, const char *tunnel_id)
{
	tunnel_t *tunnel = NULL;

	tunnel = (tunnel_t *)malloc(sizeof(tunnel_t));
	if (tunnel == NULL) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	tunnel->tunnel_id = strdup(tunnel_id);
	tunnel->link = link;
	tunnel->type = type;
	tunnel->state = state;
	tunnel->ref_count = 0;

	return tunnel;
}

tunnel_t *create_tunnel(link_t *link, tunnel_type_t type, tunnel_state_t state)
{
	tunnel_t *tunnel = NULL;
	char *tunnel_id = NULL;

	tunnel_id = gen_uuid("TUNNEL_");
	if (tunnel_id == NULL) {
		log_error("Failed to generate id");
		return NULL;
	}

	tunnel = create_tunnel_from_id(link, type, state, tunnel_id);

	free(tunnel_id);

	return tunnel;
}

result_t add_tunnel(node_t *node, tunnel_t *tunnel)
{
	int i_tunnel = 0;

	for (i_tunnel = 0; i_tunnel < MAX_TUNNELS; i_tunnel++) {
		if (node->tunnels[i_tunnel] == NULL) {
			node->tunnels[i_tunnel] = tunnel;
			log_debug("Tunnel '%s' added", tunnel->tunnel_id);
			return SUCCESS;
		}
	}

	log_error("Failed to add tunnel '%s'", tunnel->tunnel_id);

	return FAIL;
}

result_t remove_tunnel(node_t *node, const char *tunnel_id)
{
	int i_tunnel = 0;

	for (i_tunnel = 0; i_tunnel < MAX_TUNNELS; i_tunnel++) {
		if (node->tunnels[i_tunnel] != NULL) {
			if (strcmp(node->tunnels[i_tunnel]->tunnel_id, tunnel_id) == 0) {
				log_debug("Tunnel '%s' removed", tunnel_id);
				node->tunnels[i_tunnel] = NULL;
				return SUCCESS;
			}
		}
	}

	return FAIL;
}

static result_t destroy_tunnel_handler(char *data, void *msg_data)
{
	log_debug("destroy_tunnel_handler does nothing");
	return SUCCESS;
}

void free_tunnel(node_t *node, tunnel_t *tunnel)
{
	result_t result = FAIL;

	if (tunnel != NULL) {
		log_debug("Destroying tunnel '%s'", tunnel->tunnel_id);
		result = send_tunnel_destroy(node, tunnel->link->peer_id, tunnel->tunnel_id, destroy_tunnel_handler);
		if (result != SUCCESS)
			log_error("Failed to send tunnel destroy");
		free(tunnel->tunnel_id);
		free(tunnel);
	}
}

void tunnel_add_ref(tunnel_t *tunnel)
{
	if (tunnel != NULL) {
		tunnel->ref_count++;
		log_debug("Tunnel ref added '%s' ref: %d", tunnel->tunnel_id, tunnel->ref_count);
	}
}

void tunnel_remove_ref(node_t *node, tunnel_t *tunnel)
{
	if (tunnel != NULL) {
		tunnel->ref_count--;
		log_debug("Tunnel ref removed '%s' ref: %d", tunnel->tunnel_id, tunnel->ref_count);
		if (tunnel->ref_count == 0) {
			remove_tunnel(node, tunnel->tunnel_id);
			free_tunnel(node, tunnel);
		}
	}
}

tunnel_t *get_tunnel(node_t *node, const char *tunnel_id)
{
	int i_tunnel = 0;

	if (tunnel_id == NULL)
		return NULL;

	for (i_tunnel = 0; i_tunnel < MAX_TUNNELS; i_tunnel++) {
		if (node->tunnels[i_tunnel] != NULL) {
			if (strcmp(node->tunnels[i_tunnel]->tunnel_id, tunnel_id) == 0)
				return node->tunnels[i_tunnel];
		}
	}

	return NULL;
}

tunnel_t *get_tunnel_from_peerid(node_t *node, const char *peer_id)
{
	int i_tunnel = 0;

	if (peer_id == NULL)
		return NULL;

	for (i_tunnel = 0; i_tunnel < MAX_TUNNELS; i_tunnel++) {
		if (node->tunnels[i_tunnel] != NULL) {
			if (strcmp(node->tunnels[i_tunnel]->link->peer_id, peer_id) == 0)
				return node->tunnels[i_tunnel];
		}
	}

	return NULL;
}

static result_t token_tunnel_reply_handler(char *data, void *msg_data)
{
	uint32_t status;
	char *value = NULL, *tunnel_id = NULL, *data_value = NULL;
	result_t result = FAIL;
	tunnel_t *tunnel = NULL;
	node_t *node = get_node();

	result = get_value_from_map(&data, "value", &value);

	if (result == SUCCESS)
		result = get_value_from_map(&value, "data", &data_value);

	if (result == SUCCESS)
		result = decode_string_from_map(&data_value, "tunnel_id", &tunnel_id);

	if (result == SUCCESS)
		result = decode_uint_from_map(&value, "status", &status);

	if (result != SUCCESS)
		log_error("Failed to parse reply");

	if (result == SUCCESS && status == 200) {
		tunnel = get_tunnel(node, tunnel_id);
		if (tunnel != NULL) {
			log_debug("Tunnel '%s' connected", tunnel_id);
			tunnel->state = TUNNEL_WORKING;
			result = tunnel_connected(node, tunnel);
		} else
			log_error("TODO: Handle tunnel failures");
	}

	if (tunnel_id != NULL)
		free(tunnel_id);

	return result;
}

result_t request_token_tunnel(node_t *node, tunnel_t *tunnel)
{
	result_t result = FAIL;
	char *tunnel_id = gen_uuid("TUNNEL_");

	result = send_tunnel_request(node, tunnel, token_tunnel_reply_handler);
	free(tunnel_id);

	return result;
}

result_t handle_tunnel_new_request(struct node_t *node, char *peer_id, char *tunnel_id)
{
	link_t *link = NULL;
	tunnel_t *tunnel = NULL;
	char *tmp_id = NULL;

	link = get_link(node, peer_id);
	if (link == NULL) {
		log_error("No link connected to '%s'", peer_id);
		return FAIL;
	}

	tunnel = get_tunnel_from_peerid(node, peer_id);
	if (tunnel != NULL) {
		if (tunnel->state == TUNNEL_WORKING) {
			log_error("Tunnel already connected to '%s'", peer_id);
			return FAIL;
		}

		tmp_id = get_highest_uuid(tunnel_id, tunnel_id);
		if (strcmp(tunnel_id, tmp_id) == 0) {
			free(tunnel->tunnel_id);
			tunnel->tunnel_id = strdup(tunnel_id);
			tunnel->state = TUNNEL_WORKING;
		}
		return SUCCESS;
	}

	tunnel = create_tunnel_from_id(link, TUNNEL_TYPE_TOKEN, TUNNEL_WORKING, tunnel_id);
	if (tunnel != NULL) {
		if (add_tunnel(node, tunnel) != SUCCESS) {
			free_tunnel(node, tunnel);
			return FAIL;
		} else
			return SUCCESS;
	}

	return FAIL;
}
