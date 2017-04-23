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
#include "msgpuck/msgpuck.h"
#include "node.h"
#include "link.h"

tunnel_t *tunnel_get_from_id(node_t *node, const char *tunnel_id, uint32_t tunnel_id_len)
{
	return (tunnel_t *)list_get_n(node->tunnels, tunnel_id, tunnel_id_len);
}

tunnel_t *tunnel_get_from_peerid_and_type(node_t *node, const char *peer_id, uint32_t peer_id_len, tunnel_type_t type)
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

static result_t tunnel_destroy_handler(node_t *node, char *data, void *msg_data)
{
	return SUCCESS;
}

static result_t tunnel_request_handler(node_t *node, char *data, void *msg_data)
{
	uint32_t status = 0, tunnel_id_len = 0;
	char *value = NULL, *tunnel_id = NULL, *data_value = NULL;
	result_t result = FAIL;
	tunnel_t *tunnel = (tunnel_t *)msg_data;

	if (msg_data == NULL) {
		log_error("Expected msg_data");
		return FAIL;
	}

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

	strncpy(tunnel->id, tunnel_id, tunnel_id_len);

	if (status == 200) {
		log("Tunnel '%.*s' connected", (int)tunnel_id_len, tunnel_id);
		tunnel->state = TUNNEL_ENABLED;
		return SUCCESS;
	}

	log_error("Failed to connect tunnel");
	tunnel->state = TUNNEL_DISCONNECTED;
	return FAIL;
}

tunnel_t *tunnel_create(node_t *node, tunnel_type_t type, tunnel_state_t state, char *peer_id, uint32_t peer_id_len, char *tunnel_id, uint32_t tunnel_id_len)
{
	tunnel_t *tunnel = NULL;
	link_t *link = NULL;

	tunnel = tunnel_get_from_peerid_and_type(node, peer_id, peer_id_len, type);
	if (tunnel != NULL)
		return tunnel;

	link = link_get(node, peer_id, peer_id_len);
	if (link == NULL) {
		link = link_create(node, peer_id, peer_id_len, strncmp(node->proxy_link->peer_id, peer_id, peer_id_len) == 0);
		if (link == NULL) {
			log_error("Failed to create link to '%.*s'", peer_id_len, peer_id);
			return NULL;
		}
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
		gen_uuid(tunnel->id, "TUNNEL_");
	else
		strncpy(tunnel->id, tunnel_id, tunnel_id_len);

	if (state != TUNNEL_ENABLED) {
		if (proto_send_tunnel_request(node, tunnel, tunnel_request_handler) == SUCCESS)
			tunnel->state = TUNNEL_PENDING;
		else {
			platform_mem_free((void *)tunnel);
			return NULL;
		}
	}

	if (list_add(&node->tunnels, tunnel->id, (void *)tunnel, sizeof(tunnel_t)) != SUCCESS) {
		log_error("Failed to add tunnel");
		platform_mem_free((void *)tunnel);
		return NULL;
	}

	link_add_ref(link);

	log("Tunnel created, id '%s' peer_id '%s'", tunnel->id, tunnel->link->peer_id);

	return tunnel;
}

char *tunnel_serialize(const tunnel_t *tunnel, char **buffer)
{
	*buffer = mp_encode_map(*buffer, 4);
	{
		*buffer = encode_str(buffer, "id", tunnel->id, strlen(tunnel->id));
		*buffer = encode_str(buffer, "peer_id", tunnel->link->peer_id, strlen(tunnel->link->peer_id));
		*buffer = encode_uint(buffer, "state", tunnel->state);
		*buffer = encode_uint(buffer, "type", tunnel->type);
	}

	return *buffer;
}

tunnel_t *tunnel_deserialize(struct node_t *node, char *buffer)
{
	char *id = NULL, *peer_id = NULL;
	uint32_t len_id = 0, len_peer_id = 0, state = 0, type = 0;

	if (decode_string_from_map(buffer, "id", &id, &len_id) != SUCCESS)
		return NULL;

	if (decode_string_from_map(buffer, "peer_id", &peer_id, &len_peer_id) != SUCCESS)
		return NULL;

	if (decode_uint_from_map(buffer, "state", &state) != SUCCESS)
		return NULL;

	if (decode_uint_from_map(buffer, "type", &type) != SUCCESS)
		return NULL;

	return tunnel_create(node, (tunnel_type_t)type, (tunnel_state_t)state, peer_id, len_peer_id, id, len_id);
}

void tunnel_free(node_t *node, tunnel_t *tunnel)
{
	log("Deleting tunnel '%s'", tunnel->id);
	list_remove(&node->tunnels, tunnel->id);
	link_remove_ref(node, tunnel->link);
	platform_mem_free((void *)tunnel);
}

void tunnel_add_ref(tunnel_t *tunnel)
{
	if (tunnel != NULL)
		tunnel->ref_count++;
}

void tunnel_remove_ref(node_t *node, tunnel_t *tunnel)
{
	if (tunnel != NULL) {
		tunnel->ref_count--;
		log_debug("Tunnel ref removed '%s' ref: %d", tunnel->id, tunnel->ref_count);
		if (tunnel->ref_count == 0) {
			if (proto_send_tunnel_destroy(node, tunnel, tunnel_destroy_handler) != SUCCESS)
				log_error("Failed to destroy tunnel '%s'", tunnel->id);
			tunnel_free(node, tunnel);
		}
	}
}

result_t tunnel_handle_tunnel_new_request(node_t *node, char *peer_id, uint32_t peer_id_len, char *tunnel_id, uint32_t tunnel_id_len)
{
	tunnel_t *tunnel = NULL;

	tunnel = tunnel_get_from_peerid_and_type(node, peer_id, peer_id_len, TUNNEL_TYPE_TOKEN);
	if (tunnel != NULL) {
		if (tunnel->state == TUNNEL_ENABLED) {
			log_error("Tunnel already connected to '%s'", peer_id);
			return SUCCESS;
		}

		if (uuid_is_higher(tunnel_id, tunnel_id_len, tunnel->id, strlen(tunnel->id)))
			strncpy(tunnel->id, tunnel_id, tunnel_id_len);

		tunnel->state = TUNNEL_ENABLED;
	} else {
		tunnel = tunnel_create(node, TUNNEL_TYPE_TOKEN, TUNNEL_ENABLED, peer_id, peer_id_len, tunnel_id, tunnel_id_len);
		if (tunnel == NULL) {
			log_error("Failed to create tunnel");
			return FAIL;
		}
	}

	return SUCCESS;
}
