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

tunnel_t *create_tunnel_with_id(const char *peer_id, const char *tunnel_id)
{
	tunnel_t *tunnel = (tunnel_t*)malloc(sizeof(tunnel_t));
	if (tunnel == NULL) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	log_debug("Tunnel '%s' created to '%s'", tunnel_id, peer_id);

	tunnel->tunnel_id = strdup(tunnel_id);
	tunnel->peer_id = strdup(peer_id);
	tunnel->state = TUNNEL_DISCONNECTED;
	tunnel->ref_count = 0;
	return tunnel;
}

tunnel_t *create_tunnel(const char *peer_id)
{
	tunnel_t *tunnel = NULL;
	char *tunnel_id = gen_uuid("TUNNEL_");

	tunnel = create_tunnel_with_id(peer_id, tunnel_id);
	free(tunnel_id);

	return tunnel;
}

void free_tunnel(tunnel_t* tunnel)
{
	if (tunnel != NULL) {
		log_debug("Freeing tunnel '%s'", tunnel->tunnel_id);
		free(tunnel->tunnel_id);
		free(tunnel->peer_id);
		free(tunnel);
	}
}

void tunnel_client_connected(tunnel_t *tunnel)
{
	if (tunnel != NULL) {
		tunnel->ref_count++;
	}
}

void tunnel_client_disconnected(tunnel_t *tunnel)
{
	if (tunnel != NULL) {
		tunnel->ref_count--;
		if (tunnel->ref_count == 0) {
			remove_token_tunnel(tunnel->tunnel_id);
			free_tunnel(tunnel);
		}
	}
}