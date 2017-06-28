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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cc_node.h"
#include "scheduler/cc_scheduler.h"
#include "cc_proto.h"
#include "cc_transport.h"
#include "cc_msgpack_helper.h"
#include "../../msgpuck/msgpuck.h"
#include "../south/platform/cc_platform.h"
#ifdef CC_TLS_ENABLED
#include "../../crypto/cc_crypto.h"
#endif

#define CC_RECONNECT_TIMEOUT 5

static void node_reset(node_t *node, bool remove_actors)
{
	int i = 0;
	list_t *tmp_list = NULL;

	log("Resetting node");

	while (node->actors != NULL) {
		tmp_list = node->actors;
		node->actors = node->actors->next;
		if (remove_actors)
			actor_free(node, (actor_t *)tmp_list->data, true);
		else
			actor_disconnect(node, (actor_t *)tmp_list->data);
	}
	if (remove_actors)
		node->actors = NULL;

	while (node->tunnels != NULL) {
		tmp_list = node->tunnels;
		node->tunnels = node->tunnels->next;
		tunnel_free(node, (tunnel_t *)tmp_list->data);
	}
	node->tunnels = NULL;
	node->storage_tunnel = NULL;

	while (node->links != NULL) {
		tmp_list = node->links;
		node->links = node->links->next;
		link_free(node, (link_t *)tmp_list->data);
	}
	node->links = NULL;
	node->proxy_link = NULL;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		node->pending_msgs[i].handler = NULL;
		node->pending_msgs[i].msg_data = NULL;
	}
}

#ifdef CC_STORAGE_ENABLED
static result_t node_get_state(node_t *node)
{
	char buffer[CC_RUNTIME_STATE_BUFFER_SIZE], *value = NULL, *array_value = NULL;
	uint32_t i = 0, value_len = 0, array_size = 0, state = 0;
	link_t *link = NULL;
	tunnel_t *tunnel = NULL;
	actor_t *actor = NULL;

	if (platform_read_node_state(node, buffer, CC_RUNTIME_STATE_BUFFER_SIZE) == CC_RESULT_SUCCESS) {
		if (decode_string_from_map(buffer, "id", &value, &value_len) != CC_RESULT_SUCCESS) {
			log_error("Failed to decode 'id'");
			return CC_RESULT_FAIL;
		}
		strncpy(node->id, value, value_len);

		if (decode_string_from_map(buffer, "name", &value, &value_len) != CC_RESULT_SUCCESS) {
			log_error("Failed to decode 'name'");
			return CC_RESULT_FAIL;
		}
		strncpy(node->name, value, value_len);
		node->name[value_len] = '\0';

		if (has_key(buffer, "state")) {
			if (decode_uint_from_map(buffer, "state", &state) == CC_RESULT_SUCCESS)
				node->state = (node_state_t)state;
		}

		if (has_key(buffer, "proxy_uris")) {
			if (get_value_from_map(buffer, "proxy_uris", &array_value) == CC_RESULT_SUCCESS) {
				array_size = get_size_of_array(array_value);
				for (i = 0; i < array_size; i++) {
					if (decode_string_from_array(array_value, i, &value, &value_len) == CC_RESULT_SUCCESS)
						list_add_n(&node->proxy_uris, value, value_len, NULL, 0);
				}
			}
		}

		if (has_key(buffer, "links")) {
			if (get_value_from_map(buffer, "links", &array_value) == CC_RESULT_SUCCESS) {
				array_size = get_size_of_array(array_value);
				for (i = 0; i < array_size; i++) {
					if (get_value_from_array(array_value, i, &value) == CC_RESULT_SUCCESS) {
						link = link_deserialize(node, value);
						if (link == NULL)
							log_error("Failed to decode link");
						else {
							if (link->is_proxy)
								node->proxy_link = link;
						}
					}
				}
			}
		}

		if (has_key(buffer, "tunnels")) {
			if (get_value_from_map(buffer, "tunnels", &array_value) == CC_RESULT_SUCCESS) {
				array_size = get_size_of_array(array_value);
				for (i = 0; i < array_size; i++) {
					if (get_value_from_array(array_value, i, &value) == CC_RESULT_SUCCESS) {
						tunnel = tunnel_deserialize(node, value);
						if (tunnel == NULL)
						 	log_error("Failed to decode tunnel");
						else {
							if (tunnel->type == TUNNEL_TYPE_STORAGE)
								node->storage_tunnel = tunnel;
						}
					}
				}
			}
		}

		if (has_key(buffer, "actors")) {
			if (get_value_from_map(buffer, "actors", &array_value) == CC_RESULT_SUCCESS) {
				array_size = get_size_of_array(array_value);
				for (i = 0; i < array_size; i++) {
					if (get_value_from_array(array_value, i, &value) == CC_RESULT_SUCCESS) {
						actor = actor_create(node, value);
						if (actor == NULL)
							log_error("Failed to decode actor");
					}
				}
			}
		}
		return CC_RESULT_SUCCESS;
	}
	return CC_RESULT_FAIL;
}

void node_set_state(node_t *node)
{
	char buffer[CC_RUNTIME_STATE_BUFFER_SIZE];
	char *tmp = buffer;
	int nbr_of_items = 0;
	list_t *item = NULL;

	tmp = mp_encode_map(tmp, 8);
	{
		tmp = encode_uint(&tmp, "state", node->state);
		tmp = encode_str(&tmp, "id", node->id, strlen(node->id));
		tmp = encode_str(&tmp, "name", node->name, strlen(node->name));

		nbr_of_items = list_count(node->proxy_uris);
		tmp = encode_array(&tmp, "proxy_uris", nbr_of_items);
		{
			item = node->proxy_uris;
			while (item != NULL) {
				tmp = mp_encode_str(tmp, item->id, strlen(item->id));
				item = item->next;
			}
		}

		nbr_of_items = list_count(node->links);
		tmp = encode_array(&tmp, "links", nbr_of_items);
		{
			item = node->links;
			while (item != NULL) {
				tmp = link_serialize((link_t *)item->data, &tmp);
				item = item->next;
			}
		}

		nbr_of_items = list_count(node->tunnels);
		tmp = encode_array(&tmp, "tunnels", nbr_of_items);
		{
			item = node->tunnels;
			while (item != NULL) {
				tmp = tunnel_serialize((tunnel_t *)item->data, &tmp);
				item = item->next;
			}
		}

		nbr_of_items = list_count(node->actors);
		tmp = encode_array(&tmp, "actors", nbr_of_items);
		{
			item = node->actors;
			while (item != NULL) {
				tmp = mp_encode_map(tmp, 1);
				tmp = actor_serialize(node, (actor_t *)item->data, &tmp, true);
				item = item->next;
			}
		}
	}

	platform_write_node_state(node, buffer, tmp - buffer);
}
#endif

result_t node_add_pending_msg(node_t *node, char *msg_uuid, uint32_t msg_uuid_len, result_t (*handler)(node_t *node, char *data, void *msg_data), void *msg_data)
{
	int i = 0;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		if (node->pending_msgs[i].handler == NULL) {
			strncpy(node->pending_msgs[i].msg_uuid, msg_uuid, msg_uuid_len);
			node->pending_msgs[i].handler = handler;
			node->pending_msgs[i].msg_data = msg_data;
			return CC_RESULT_SUCCESS;
		}
	}

	log_error("Pending msg queue is full");
	return CC_RESULT_FAIL;
}

result_t node_remove_pending_msg(node_t *node, char *msg_uuid, uint32_t msg_uuid_len)
{
	int i = 0;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		if (node->pending_msgs[i].handler != NULL) {
			if (strncmp(node->pending_msgs[i].msg_uuid, msg_uuid, msg_uuid_len) == 0) {
				node->pending_msgs[i].handler = NULL;
				node->pending_msgs[i].msg_data = NULL;
				return CC_RESULT_SUCCESS;
			}
		}
	}

	log_error("No pending msg with id '%s'", msg_uuid);
	return CC_RESULT_FAIL;
}

result_t node_get_pending_msg(node_t *node, const char *msg_uuid, uint32_t msg_uuid_len, pending_msg_t *pending_msg)
{
	int i = 0;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		if (node->pending_msgs[i].handler != NULL) {
			if (strncmp(node->pending_msgs[i].msg_uuid, msg_uuid, msg_uuid_len) == 0) {
				*pending_msg = node->pending_msgs[i];
				return CC_RESULT_SUCCESS;
			}
		}
	}

	log_debug("No pending msg with id '%s'", msg_uuid);
	return CC_RESULT_FAIL;
}

bool node_can_add_pending_msg(const node_t *node)
{
	int i = 0;

	for (i = 0; i < MAX_PENDING_MSGS; i++) {
		if (node->pending_msgs[i].handler == NULL)
			return true;
	}

	return false;
}

static result_t node_setup_reply_handler(node_t *node, char *data, void *msg_data)
{
	uint32_t status;
	char *value = NULL;

	if (get_value_from_map(data, "value", &value) == CC_RESULT_SUCCESS) {
		if (decode_uint_from_map(value, "status", &status) == CC_RESULT_SUCCESS) {
			if (status == 200) {
				log("Connected to proxy with id '%s' and uri '%s'", node->transport_client->peer_id, node->transport_client->uri);
				node->state = NODE_STARTED;
				platform_node_started(node);
			} else
				log_error("Failed to setup node, status '%lu'", (unsigned long)status);
			return CC_RESULT_SUCCESS;
		}
	}

	log_error("Failed to decode PROXY_CONFIG reply");
	return CC_RESULT_FAIL;
}

#ifdef CC_DEEPSLEEP_ENABLED
static result_t node_enter_sleep_reply_handler(node_t *node, char *data, void *msg_data)
{
	uint32_t status;
	char *value = NULL;

	if (get_value_from_map(data, "value", &value) == CC_RESULT_SUCCESS) {
		if (decode_uint_from_map(value, "status", &status) == CC_RESULT_SUCCESS) {
			if (status == 200) {
				log("Node going to deep sleep");
#ifdef CC_STORAGE_ENABLED
				node_set_state(node);
#else
				// TODO: Move actors to proxy while sleeping and when notifying proxy
				// of wake up, move actors back
#endif
				node->state = NODE_STOP;
				platform_deepsleep(node);
			} else
				log_error("Failed to request sleep");
			return CC_RESULT_SUCCESS;
		}
	}

	log_error("Failed to decode PROXY_CONFIG reply for enterring deep sleep");
	return CC_RESULT_FAIL;
}
#endif

result_t node_handle_token(port_t *port, const char *data, const size_t size, uint32_t sequencenbr)
{
	char *buffer = NULL;

	if (port->actor->state == ACTOR_ENABLED) {
		if (fifo_slots_available(port->fifo, 1)) {
			if (platform_mem_alloc((void **)&buffer, size) != CC_RESULT_SUCCESS) {
				log_error("Failed to allocate memory");
				return CC_RESULT_FAIL;
			}
			memcpy(buffer, data, size);
			if (fifo_com_write(port->fifo, buffer, size, sequencenbr) == CC_RESULT_SUCCESS)
				return CC_RESULT_SUCCESS;
			else {
				log_error("Failed to write to fifo");
				platform_mem_free((void *)buffer);
			}
		} else
			log("Token received but no slots available");
	} else
		log("Token received but actor not enabled");

	return CC_RESULT_FAIL;
}

void node_handle_token_reply(node_t *node, char *port_id, uint32_t port_id_len, port_reply_type_t reply_type, uint32_t sequencenbr)
{
	port_t *port = port_get(node, port_id, port_id_len);

	if (port != NULL) {
		if (reply_type == PORT_REPLY_TYPE_ACK) {
			fifo_com_commit_read(port->fifo, sequencenbr);
		} else if (reply_type == PORT_REPLY_TYPE_NACK) {
			fifo_com_cancel_read(port->fifo, sequencenbr);
		} else if (reply_type == PORT_REPLY_TYPE_ABORT)
			log_debug("TODO: handle ABORT");
	}
}

result_t node_handle_message(node_t *node, char *buffer, size_t len)
{
	if (proto_parse_message(node, buffer) == CC_RESULT_SUCCESS) {
#if defined(CC_STORAGE_ENABLED) && defined(CC_STATE_CHECKPOINTING)
		// message successfully handled == state changed -> serialize the node
		if (node->state == NODE_STARTED)
			node_set_state(node);
#endif
		return CC_RESULT_SUCCESS;
	}

	log_error("Failed to handle message");
	return CC_RESULT_FAIL;
}

static result_t node_setup(node_t *node, char *name)
{
#ifdef CC_TLS_ENABLED
	char domain[50];

	if (crypto_get_node_info(domain, node->name, node->id) == CC_RESULT_SUCCESS)
		return CC_RESULT_SUCCESS;
#endif

#ifdef CC_STORAGE_ENABLED
	if (node_get_state(node) == CC_RESULT_SUCCESS)
		return CC_RESULT_SUCCESS;
#endif

	gen_uuid(node->id, NULL);

	if (name != NULL)
		strncpy(node->name, name, strlen(name) + 1);
	else
		strncpy(node->name, "constrained", 12);

	return CC_RESULT_SUCCESS;
}

static result_t node_connect_to_proxy(node_t *node, char *uri)
{
	char *peer_id = NULL;
	size_t peer_id_len = 0;

	if (node->transport_client == NULL) {
		node->transport_client = transport_create(node, uri);
		if (node->transport_client == NULL)
			return CC_RESULT_FAIL;
	}

	while (node->state != NODE_STOP && node->transport_client->state == TRANSPORT_INTERFACE_DOWN)
		platform_evt_wait(node, 0);

	if (node->state == NODE_STOP || node->transport_client->connect(node, node->transport_client) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	while (node->state != NODE_STOP && node->transport_client->state == TRANSPORT_PENDING)
		platform_evt_wait(node, 0);

	if (transport_join(node, node->transport_client) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	while (node->state != NODE_STOP && node->transport_client->state == TRANSPORT_PENDING)
		platform_evt_wait(node, 0);

	if (node->state == NODE_STOP || node->transport_client->state != TRANSPORT_ENABLED)
		return CC_RESULT_FAIL;

	peer_id = node->transport_client->peer_id;
	peer_id_len = strlen(peer_id);

	if (node->proxy_link != NULL && strncmp(node->proxy_link->peer_id, peer_id, peer_id_len) != 0)
		node_reset(node, false);

	if (node->proxy_link == NULL) {
		node->proxy_link = link_create(node, peer_id, peer_id_len, true);
		if (node->proxy_link == NULL) {
			log_error("Failed to create proxy link");
			return CC_RESULT_FAIL;
		}
	}

	if (node->storage_tunnel == NULL) {
		node->storage_tunnel = tunnel_create(node, TUNNEL_TYPE_STORAGE, TUNNEL_DISCONNECTED, peer_id, peer_id_len, NULL, 0);
		if (node->storage_tunnel == NULL) {
			log_error("Failed to create storage tunnel");
			return CC_RESULT_FAIL;
		}
		tunnel_add_ref(node->storage_tunnel);
	}

	if (proto_send_node_setup(node, false, node_setup_reply_handler) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	while (node->state != NODE_STARTED && node->state != NODE_STOP)
		platform_evt_wait(node, 0);

	if (node->state != NODE_STARTED) {
		log_error("Failed connect to proxy");
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

result_t node_init(node_t *node, char *name, char *proxy_uris)
{
	char *uri = NULL;
	list_t *item = NULL;

	node->state = NODE_DO_START;
	node->fire_actors = fire_actors;
	node->transport_client = NULL;
	node->proxy_link = NULL;
	node->platform = NULL;
	node->attributes = NULL;
	node->links = NULL;
	node->storage_tunnel = NULL;
	node->tunnels = NULL;
	node->actors = NULL;

	if (platform_create(node) != CC_RESULT_SUCCESS) {
		log_error("Failed to create platform object");
		return CC_RESULT_FAIL;
	}

	if (platform_mem_alloc((void **)&node->calvinsys, sizeof(calvinsys_t)) != CC_RESULT_SUCCESS) {
		log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	node->calvinsys->node = node;
	node->calvinsys->capabilities = NULL;
	node->calvinsys->handlers = NULL;

	if (platform_create_calvinsys(&node->calvinsys) != CC_RESULT_SUCCESS) {
		log_error("Failed to create calvinsys");
		return CC_RESULT_FAIL;
	}

	if (node_setup(node, name) != CC_RESULT_SUCCESS) {
		log_error("Failed to setup node");
		return CC_RESULT_FAIL;
	}

	if (proxy_uris != NULL) {
		uri = strtok(proxy_uris, " ");
		while (uri != NULL) {
			list_add(&node->proxy_uris, uri, NULL, 0);
			uri = strtok(NULL, " ");
		}
	}

	log("Node initialized");
	log("----------------------------------------");
	log("Name: %s", node->name);
	log("ID: %s", node->id);
	log("Proxy URIs:");
	if (node->proxy_uris != NULL) {
		item = node->proxy_uris;
		while (item != NULL) {
			log(" %s", item->id);
			item = item->next;
		}
	}
	log("Capabilities:");
	if (node->calvinsys != NULL) {
		item = node->calvinsys->capabilities;
		while (item != NULL) {
			log(" %s", item->id);
			item = item->next;
		}
	}
	if (node->storage_dir != NULL)
	log("Storage directory: %s", node->storage_dir);
	log("----------------------------------------");

	return CC_RESULT_SUCCESS;
}

static void node_free(node_t *node)
{
	list_t *item = NULL, *tmp_item = NULL;
	calvinsys_handler_t *handler = NULL;

	item = node->actors;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		actor_free(node, (actor_t *)tmp_item->data, false);
	}

	item = node->tunnels;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		tunnel_free(node, (tunnel_t *)tmp_item->data);
	}

	item = node->links;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		link_free(node, (link_t *)tmp_item->data);
	}

	platform_stop(node);

	if (node->attributes != NULL) {
		item = node->attributes->indexed_public_node_name;
		while (item != NULL) {
			tmp_item = node->attributes->indexed_public_node_name;
			item = item->next;
			list_remove(&node->attributes->indexed_public_node_name, tmp_item->id);
		}
		platform_mem_free((void *)node->attributes);
	}

	if (node->platform != NULL)
		platform_mem_free((void *)node->platform);

	item = node->calvinsys->capabilities;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		platform_mem_free((void *)tmp_item->id);
		platform_mem_free((void *)tmp_item);
	}

	while (node->calvinsys->handlers != NULL) {
		handler = node->calvinsys->handlers;
		node->calvinsys->handlers = node->calvinsys->handlers->next;
		calvinsys_delete_handler(handler);
	}
	platform_mem_free((void *)node->calvinsys);

	platform_mem_free((void *)node);
}

result_t node_run(node_t *node)
{
	list_t *item = NULL;

	if (node->fire_actors == NULL) {
		log_error("No actor scheduler set");
		return CC_RESULT_FAIL;
	}

	while (node->state != NODE_STOP) {
		item = node->proxy_uris;
		while (item != NULL && node->state != NODE_STOP) {
			node->state = NODE_DO_START;
			if (node_connect_to_proxy(node, item->id) == CC_RESULT_SUCCESS) {
				while (node->state != NODE_STOP && node->transport_client->state == TRANSPORT_ENABLED) {
					if (node->state == NODE_STARTED)
						node->fire_actors(node);
#ifdef CC_DEEPSLEEP_ENABLED
					if (!platform_evt_wait(node, CC_INACTIVITY_TIMEOUT)) {
						if (proto_send_node_setup(node, true, node_enter_sleep_reply_handler) == CC_RESULT_SUCCESS)
							node->state = NODE_PENDING;
					}
#else
					platform_evt_wait(node, 0);
#endif
				}
			}

			if (node->transport_client != NULL) {
				node->transport_client->disconnect(node, node->transport_client);
				node->transport_client->free(node->transport_client);
				node->transport_client = NULL;
			}
			item = item->next;
		}

		if (node->state != NODE_STOP) {
			// TODO: If reconnect fails goto sleep after x retries
			platform_evt_wait(node, 5);
		}
	}

	node_free(node);

	log("Node stopped");
	return CC_RESULT_SUCCESS;
}
