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
#include "cc_tunnel.h"
#include "cc_proto.h"
#include "cc_node.h"
#include "cc_link.h"
#include "coder/cc_coder.h"
#include "../south/platform/cc_platform.h"

cc_tunnel_t *cc_tunnel_get_from_id(cc_node_t *node, const char *tunnel_id, uint32_t tunnel_id_len)
{
	cc_list_t *item = NULL;

	item = cc_list_get_n(node->tunnels, tunnel_id, tunnel_id_len);
	if (item != NULL)
		return (cc_tunnel_t *)item->data;

	return NULL;
}

cc_tunnel_t *cc_tunnel_get_from_peerid_and_type(cc_node_t *node, const char *peer_id, uint32_t peer_id_len, cc_tunnel_type_t type)
{
	cc_list_t *tunnels = node->tunnels;
	cc_tunnel_t *tunnel = NULL;

	while (tunnels != NULL) {
		tunnel = (cc_tunnel_t *)tunnels->data;
		if (tunnel == NULL || tunnel->link == NULL || tunnel->link->peer_id == NULL)
			continue;
		if (tunnel->type == type && strncmp(tunnel->link->peer_id, peer_id, peer_id_len) == 0)
			return tunnel;
		tunnels = tunnels->next;
	}

	return NULL;
}

static cc_result_t tunnel_destroy_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	return CC_SUCCESS;
}

static cc_result_t tunnel_request_handler(cc_node_t *node, char *data, size_t data_len, void *msg_data)
{
	uint32_t status = 0, tunnel_id_len = 0;
	char *value = NULL, *tunnel_id = NULL, *data_value = NULL;
	cc_result_t result = CC_FAIL;
	cc_tunnel_t *tunnel = NULL;


	result = cc_coder_get_value_from_map(data, "value", &value);
	if (result == CC_SUCCESS)
		result = cc_coder_get_value_from_map(value, "data", &data_value);

	if (result == CC_SUCCESS)
		result = cc_coder_decode_string_from_map(data_value, "tunnel_id", &tunnel_id, &tunnel_id_len);

	if (result == CC_SUCCESS)
		result = cc_coder_decode_uint_from_map(value, "status", &status);

	if (result != CC_SUCCESS) {
		cc_log_error("Failed to parse reply");
		return CC_FAIL;
	}

	tunnel = cc_tunnel_get_from_id(node, tunnel_id, tunnel_id_len);

	if (status == 200) {
		if (tunnel != NULL) {
			strncpy(tunnel->id, tunnel_id, tunnel_id_len);
			tunnel->id[tunnel_id_len] = '\0';
			tunnel->state = CC_TUNNEL_ENABLED;
			cc_log_debug("Tunnel '%s' connected", tunnel->id);
		}
	} else {
		if (tunnel != NULL) {
			cc_log_error("Failed to connect tunnel");
			tunnel->state = CC_TUNNEL_DISCONNECTED;
		}
	}

	return CC_SUCCESS;
}

cc_tunnel_t *cc_tunnel_create(cc_node_t *node, cc_tunnel_type_t type, cc_tunnel_state_t state, char *peer_id, uint32_t peer_id_len, char *tunnel_id, uint32_t tunnel_id_len)
{
	cc_tunnel_t *tunnel = NULL;
	cc_link_t *link = NULL;

	tunnel = cc_tunnel_get_from_peerid_and_type(node, peer_id, peer_id_len, type);
	if (tunnel != NULL)
		return tunnel;

	link = cc_link_get(node, peer_id, peer_id_len);
	if (link == NULL) {
		link = cc_link_create(node, peer_id, peer_id_len, strncmp(node->proxy_link->peer_id, peer_id, peer_id_len) == 0);
		if (link == NULL) {
			cc_log_error("Failed to create link");
			return NULL;
		}
	}

	if (cc_platform_mem_alloc((void **)&tunnel, sizeof(cc_tunnel_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	memset(tunnel, 0, sizeof(cc_tunnel_t));

	tunnel->link = link;
	tunnel->type = type;
	tunnel->state = state;
	tunnel->ref_count = 0;
	if (tunnel_id == NULL)
		cc_gen_uuid(tunnel->id, "TUNNEL_");
	else {
		strncpy(tunnel->id, tunnel_id, tunnel_id_len);
		tunnel->id[tunnel_id_len] = '\0';
	}

	if (state != CC_TUNNEL_ENABLED) {
		if (cc_proto_send_tunnel_request(node, tunnel, tunnel_request_handler) == CC_SUCCESS)
			tunnel->state = CC_TUNNEL_PENDING;
		else {
			cc_platform_mem_free((void *)tunnel);
			return NULL;
		}
	}

	if (cc_list_add(&node->tunnels, tunnel->id, (void *)tunnel, sizeof(cc_tunnel_t)) == NULL) {
		cc_log_error("Failed to add tunnel");
		cc_platform_mem_free((void *)tunnel);
		return NULL;
	}

	cc_link_add_ref(link);

	cc_log_debug("Tunnel created, id '%s' peer_id '%s'", tunnel->id, tunnel->link->peer_id);

	return tunnel;
}

char *cc_tunnel_serialize(const cc_tunnel_t *tunnel, char *buffer)
{
	buffer = cc_coder_encode_map(buffer, 4);
	{
		buffer = cc_coder_encode_kv_str(buffer, "id", tunnel->id, strnlen(tunnel->id, CC_UUID_BUFFER_SIZE));
		buffer = cc_coder_encode_kv_str(buffer, "peer_id", tunnel->link->peer_id, strnlen(tunnel->link->peer_id, CC_UUID_BUFFER_SIZE));
		buffer = cc_coder_encode_kv_uint(buffer, "state", tunnel->state);
		buffer = cc_coder_encode_kv_uint(buffer, "type", tunnel->type);
	}

	return buffer;
}

cc_tunnel_t *cc_tunnel_deserialize(struct cc_node_t *node, char *buffer)
{
	char *id = NULL, *peer_id = NULL;
	uint32_t len_id = 0, len_peer_id = 0, state = 0, type = 0;

	if (cc_coder_decode_string_from_map(buffer, "id", &id, &len_id) != CC_SUCCESS)
		return NULL;

	if (cc_coder_decode_string_from_map(buffer, "peer_id", &peer_id, &len_peer_id) != CC_SUCCESS)
		return NULL;

	if (cc_coder_decode_uint_from_map(buffer, "state", &state) != CC_SUCCESS)
		return NULL;

	if (cc_coder_decode_uint_from_map(buffer, "type", &type) != CC_SUCCESS)
		return NULL;

	return cc_tunnel_create(node, (cc_tunnel_type_t)type, (cc_tunnel_state_t)state, peer_id, len_peer_id, id, len_id);
}

void cc_tunnel_free(cc_node_t *node, cc_tunnel_t *tunnel)
{
	cc_log_debug("Deleting tunnel '%s'", tunnel->id);
	cc_list_remove(&node->tunnels, tunnel->id);
	cc_link_remove_ref(node, tunnel->link);
	cc_platform_mem_free((void *)tunnel);
}

void cc_tunnel_add_ref(cc_tunnel_t *tunnel)
{
	if (tunnel != NULL)
		tunnel->ref_count++;
}

void cc_tunnel_remove_ref(cc_node_t *node, cc_tunnel_t *tunnel)
{
	if (tunnel != NULL) {
		tunnel->ref_count--;
		cc_log_debug("Tunnel ref removed '%s' ref: %d", tunnel->id, tunnel->ref_count);
		if (tunnel->ref_count == 0) {
			if (cc_proto_send_tunnel_destroy(node, tunnel, tunnel_destroy_handler) != CC_SUCCESS)
				cc_log_debug("Failed to destroy tunnel '%s'", tunnel->id);
			cc_tunnel_free(node, tunnel);
		}
	}
}

cc_result_t cc_tunnel_handle_tunnel_new_request(cc_node_t *node, char *peer_id, uint32_t peer_id_len, char *tunnel_id, uint32_t tunnel_id_len)
{
	cc_tunnel_t *tunnel = NULL;

	tunnel = cc_tunnel_get_from_peerid_and_type(node, peer_id, peer_id_len, CC_TUNNEL_TYPE_TOKEN);
	if (tunnel != NULL) {
		if (tunnel->state == CC_TUNNEL_ENABLED) {
			cc_log_error("Tunnel already connected to '%s'", peer_id);
			return CC_SUCCESS;
		}

		if (cc_uuid_is_higher(tunnel_id, tunnel_id_len, tunnel->id, strnlen(tunnel->id, CC_UUID_BUFFER_SIZE)))
			strncpy(tunnel->id, tunnel_id, tunnel_id_len);

		tunnel->state = CC_TUNNEL_ENABLED;
	} else {
		tunnel = cc_tunnel_create(node, CC_TUNNEL_TYPE_TOKEN, CC_TUNNEL_ENABLED, peer_id, peer_id_len, tunnel_id, tunnel_id_len);
		if (tunnel == NULL) {
			cc_log_error("Failed to create tunnel");
			return CC_FAIL;
		}
	}

	return CC_SUCCESS;
}
