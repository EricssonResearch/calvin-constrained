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
#include "node.h"
#include "proto.h"

#define TOKEN_TUNNEL "token"

tunnel_t *create_tunnel(char *peer_id, transport_client_t *connection)
{
	tunnel_t *tunnel = (tunnel_t*)malloc(sizeof(tunnel_t));
	if (tunnel == NULL) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	tunnel->tunnel_id = gen_uuid("TUNNEL_");
	tunnel->peer_id = strdup(peer_id);
	tunnel->connection = connection;
	tunnel->state = TUNNEL_DISCONNECTED;
	tunnel->ref_count = 0;
	return tunnel;
}

void free_tunnel(tunnel_t* tunnel)
{
	if (tunnel != NULL) {
		free(tunnel->tunnel_id);
		free(tunnel->peer_id);
		free(tunnel);
	}
}

tunnel_t *get_token_tunnel(char *peer_id)
{
	int i_tunnel = 0;
	char *msg_uuid = NULL;
	tunnel_t *tunnel = NULL;
	node_t *node = get_node();

	// Look for existing tunnel
	for (i_tunnel = 0; i_tunnel < MAX_TUNNELS; i_tunnel++) {
		if (node->tunnels[i_tunnel] != NULL) {
			if (strcmp(node->tunnels[i_tunnel]->peer_id, peer_id) == 0) {
				return node->tunnels[i_tunnel];
			}
		}
	}

	// Create new tunnel
	for (i_tunnel = 0; i_tunnel < MAX_TUNNELS; i_tunnel++) {
		if (node->tunnels[i_tunnel] == NULL) {
			tunnel = create_tunnel(peer_id, node->client);
			if (tunnel != NULL) {
				msg_uuid = gen_uuid("MSGID_");
				if (send_tunnel_request(msg_uuid, tunnel, node->node_id, TOKEN_TUNNEL) != SUCCESS) {
					log_error("Failed to send tunnel request");
					free(msg_uuid);
					free_tunnel(tunnel);
					tunnel = NULL;
				} else {
					add_pending_msg(TUNNEL_NEW, msg_uuid);
					node->tunnels[i_tunnel] = tunnel;
					return node->tunnels[i_tunnel];
				}
			} else {
				log_error("Failed to create tunnel");
			}
		}
	}

	log_error("Failed to add tunnel");

	return NULL;
}

result_t tunnel_connected(char *tunnel_id)
{
	result_t result = SUCCESS;
	int i_tunnel = 0, i_actor = 0;
	node_t *node = get_node();

	if (strcmp(tunnel_id, node->storage_tunnel->tunnel_id) == 0) {
		log_debug("Storage tunnel connected");
		node->storage_tunnel->state = TUNNEL_CONNECTED;
	} else {
		for (i_tunnel = 0; i_tunnel < MAX_TUNNELS; i_tunnel++) {
			if (node->tunnels[i_tunnel] != NULL) {
				if (strcmp(node->tunnels[i_tunnel]->tunnel_id, tunnel_id) == 0) {
					log_debug("Tunnel '%s' connected", tunnel_id);
					node->tunnels[i_tunnel]->state = TUNNEL_CONNECTED;

					for (i_actor = 0; i_actor < MAX_ACTORS && result == SUCCESS; i_actor++) {
						if (node->actors[i_actor] != NULL) {
							result = connect_actor(node->actors[i_actor], tunnel_id);
						}
					}
					break;
				}
			}
		}
	}

	return result;
}