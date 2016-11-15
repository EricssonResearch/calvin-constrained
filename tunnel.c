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

tunnel_t *tunnel_get_from_id(node_t *node, const char *tunnel_id, uint32_t tunnel_id_len, tunnel_type_t type)
{
	list_t *tunnels = node->tunnels;
	tunnel_t *tunnel = NULL;

	while (tunnels != NULL) {
		tunnel = (tunnel_t *)tunnels->data;
		if (tunnel->type == type && strncmp(tunnel->tunnel_id, tunnel_id, tunnel_id_len) == 0)
			return tunnel;
		tunnels = tunnels->next;
	}

	return NULL;
}

tunnel_t *tunnel_get_from_peerid(node_t *node, const char *peer_id, uint32_t peer_id_len, tunnel_type_t type)
{
	list_t *tunnels = node->tunnels;
	tunnel_t *tunnel = NULL;

	while (tunnels != NULL) {
		tunnel = (tunnel_t *)tunnels->data;
		if (tunnel->type == type && strncmp(tunnel->link->peer_id, peer_id, peer_id_len) == 0)
			return tunnel;
		tunnels = tunnels->next;
	}

	return NULL;
}

static result_t tunnel_destroy_handler(char *data, void *msg_data)
{
	node_t *node = node_get();
	list_t *tunnel = NULL;

	tunnel = list_get(node->tunnels, (char *)msg_data);
	if (tunnel != NULL) {
		tunnel_free(node, (tunnel_t *)tunnel->data);
		return SUCCESS;
	}

	return FAIL;
}

tunnel_t *tunnel_create(node_t *node, tunnel_type_t type, tunnel_state_t state, char *peer_id, uint32_t peer_id_len, char *tunnel_id, uint32_t tunnel_id_len)
{
	tunnel_t *tunnel = NULL;
	link_t *link = NULL;

	link = link_get(node, peer_id, peer_id_len);
	if (link == NULL) {
		log_error("Tunnel requested without link");
		return NULL;
	}

	if (platform_mem_alloc((void **)&tunnel, sizeof(tunnel_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	memset(tunnel, 0, sizeof(tunnel_t));

	tunnel->link = link;
	tunnel->type = type;
	tunnel->state = state;
	tunnel->ref_count = 0;
	if (tunnel_id == NULL)
		gen_uuid(tunnel->tunnel_id, "TUNNEL_");
	else
		strncpy(tunnel->tunnel_id, tunnel_id, tunnel_id_len);

	if (list_add(&node->tunnels, tunnel->tunnel_id, (void *)tunnel, sizeof(tunnel_t)) != SUCCESS) {
		log_error("Failed to add tunnel");
		platform_mem_free((void *)tunnel);
		return NULL;
	}

	log_debug("Tunnel created to '%s'", tunnel->link->peer_id);

	return tunnel;
}

void tunnel_free(node_t *node, tunnel_t *tunnel)
{
	log_debug("Freeing tunnel '%s'", tunnel->tunnel_id);
	list_remove(&node->tunnels, tunnel->tunnel_id);
	platform_mem_free((void *)tunnel);
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
		if (tunnel->ref_count == 0)
			tunnel->state = TUNNEL_DO_DISCONNECT;
	}
}

static result_t tunnel_request_handler(char *data, void *msg_data)
{
	uint32_t status = 0, tunnel_id_len = 0;
	char *value = NULL, *tunnel_id = NULL, *data_value = NULL;
	result_t result = FAIL;
	tunnel_t *tunnel = NULL;
	node_t *node = node_get();

	result = get_value_from_map(data, "value", &value);
	if (result == SUCCESS)
		result = get_value_from_map(value, "data", &data_value);

	if (result == SUCCESS)
		result = decode_string_from_map(data_value, "tunnel_id", &tunnel_id, &tunnel_id_len);

	if (result == SUCCESS)
		result = decode_uint_from_map(value, "status", &status);

	if (result != SUCCESS) {
		log_error("Failed to parse reply");
		return FAIL;
	}

	tunnel = tunnel_get_from_id(node, tunnel_id, tunnel_id_len, TUNNEL_TYPE_TOKEN);
	if (tunnel == NULL) {
		log_error("No tunnel with id '%.*s'", (int)tunnel_id_len, tunnel_id);
		return FAIL;
	}

	if (status == 200) {
		log_debug("Tunnel '%.*s' connect", (int)tunnel_id_len, tunnel_id);
		tunnel->state = TUNNEL_ENABLED;
		return SUCCESS;
	}

	log_error("Failed to connect tunnel");
	tunnel->state = TUNNEL_CONNECT_FAILED;
	return FAIL;
}

result_t tunnel_handle_tunnel_new_request(node_t *node, char *peer_id, uint32_t peer_id_len, char *tunnel_id, uint32_t tunnel_id_len)
{
	tunnel_t *tunnel = NULL;

	tunnel = tunnel_get_from_peerid(node, peer_id, peer_id_len, TUNNEL_TYPE_TOKEN);
	if (tunnel != NULL) {
		if (tunnel->state == TUNNEL_ENABLED) {
			log_error("Tunnel already connected to '%s'", peer_id);
			return SUCCESS;
		}

		if (uuid_is_higher(tunnel_id, tunnel_id_len, tunnel->tunnel_id, strlen(tunnel->tunnel_id)))
			strncpy(tunnel->tunnel_id, tunnel_id, tunnel_id_len);

		tunnel->state = TUNNEL_ENABLED;
		log_debug("Tunnel '%s' enabled", tunnel->tunnel_id);
	} else {
		tunnel = tunnel_create(node, TUNNEL_TYPE_TOKEN, TUNNEL_ENABLED, peer_id, peer_id_len, tunnel_id, tunnel_id_len);
		if (tunnel == NULL) {
			log_error("Failed to create tunnel");
			return FAIL;
		}
	}

	return SUCCESS;
}

result_t tunnel_transmit(node_t *node, tunnel_t *tunnel)
{
	switch (tunnel->state) {
	case TUNNEL_DO_CONNECT:
		tunnel->state = TUNNEL_PENDING;
		if (proto_send_tunnel_request(node, tunnel, tunnel_request_handler) == SUCCESS)
			return SUCCESS;
		tunnel->state = TUNNEL_DO_CONNECT;
		break;
	case TUNNEL_DO_DISCONNECT:
		tunnel->state = TUNNEL_PENDING;
		if (proto_send_tunnel_destroy(node, tunnel, tunnel_destroy_handler) == SUCCESS)
			return SUCCESS;
		tunnel->state = TUNNEL_DO_DISCONNECT;
		break;
	case TUNNEL_CONNECT_FAILED:
		log_error("TODO: Handle failed tunnel connections");
		break;
	default:
		break;
	}

	return FAIL;
}
