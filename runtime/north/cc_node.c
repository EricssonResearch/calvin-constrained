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
#include "cc_actor_store.h"
#include "scheduler/cc_scheduler.h"
#include "cc_proto.h"
#include "cc_transport.h"
#include "cc_msgpack_helper.h"
#include "../../msgpuck/msgpuck.h"
#include "../south/platform/cc_platform.h"
#ifdef CC_TLS_ENABLED
#include "../../crypto/cc_crypto.h"
#endif
#include "../../calvinsys/common/cc_calvinsys_timer.h"

#define CC_RECONNECT_TIMEOUT	5
#ifndef CC_INACTIVITY_TIMEOUT
#define CC_INACTIVITY_TIMEOUT	1
#endif

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
			cc_log_error("Failed to decode 'id'");
			return CC_RESULT_FAIL;
		}
		strncpy(node->id, value, value_len);

#ifdef CC_TRACK_TIME
		if (has_key(buffer, "seconds")) {
			if (decode_uint_from_map(buffer, "seconds", &node->seconds) != CC_RESULT_SUCCESS)
				cc_log_error("Failed to decode 'seconds'");
		}
#endif

		if (node->attributes == NULL && has_key(buffer, "attributes")) {
			if (decode_string_from_map(buffer, "attributes", &value, &value_len) != CC_RESULT_SUCCESS) {
				cc_log_error("Failed to decode 'attributes'");
				return CC_RESULT_FAIL;
			}

			if (platform_mem_alloc((void **)&node->attributes, value_len + 1) != CC_RESULT_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				return CC_RESULT_FAIL;
			}

			strncpy(node->attributes, value, value_len);
			node->attributes[value_len] = '\0';
		}

		if (has_key(buffer, "state")) {
			if (decode_uint_from_map(buffer, "state", &state) == CC_RESULT_SUCCESS)
				node->state = (node_state_t)state;
		}

		if (has_key(buffer, "proxy_uris") && node->proxy_uris == NULL) {
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
							cc_log_error("Failed to decode link");
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
							cc_log_error("Failed to decode tunnel");
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
							cc_log_error("Failed to decode actor");
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
#ifdef CC_TRACK_TIME
	int nbr_of_items = 9;
#else
	int nbr_of_items = 8;
#endif
	list_t *item = NULL;

	tmp = mp_encode_map(tmp, nbr_of_items);
	{
		tmp = encode_uint(&tmp, "state", node->state);
		tmp = encode_str(&tmp, "id", node->id, strlen(node->id));
		tmp = encode_str(&tmp, "attributes", node->attributes, strlen(node->attributes));
#ifdef CC_TRACK_TIME
		tmp = encode_uint(&tmp, "seconds", platform_get_seconds(node) + node->seconds_of_sleep);
#endif

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

static void node_reset(node_t *node, bool remove_actors)
{
	int i = 0;
	list_t *tmp_list = NULL;
	calvinsys_handler_t *handler = NULL;
	calvinsys_obj_t *object = NULL, *tmp_obj = NULL;

	cc_log("Resetting runtime");

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

	if (node->calvinsys != NULL) {
		handler = node->calvinsys->handlers;
		while (handler != NULL) {
			object = handler->objects;
			if (object != NULL) {
				platform_mem_free((void *)tmp_obj);
			}
			handler->objects = NULL;
			handler = handler->next;
		}
	}

#ifdef CC_TRACK_TIME
	node->seconds = 0;
	node->seconds_of_sleep = 0;
#endif

#ifdef CC_STORAGE_ENABLED
	node_set_state(node);
#endif
}

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

	cc_log_error("Pending msg queue is full");
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

	cc_log_error("No pending msg with id '%s'", msg_uuid);
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

	cc_log_debug("No pending msg with id '%s'", msg_uuid);
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
				cc_log("Connected to proxy with id '%s' and uri '%s'", node->transport_client->peer_id, node->transport_client->uri);
				node->state = NODE_STARTED;
				platform_node_started(node);
			} else
				cc_log_error("Failed to setup node, status '%lu'", (unsigned long)status);
			return CC_RESULT_SUCCESS;
		}
	}

	cc_log_error("Failed to decode PROXY_CONFIG reply");
	return CC_RESULT_FAIL;
}

#ifdef CC_DEEPSLEEP_ENABLED
static result_t node_enter_sleep_reply_handler(node_t *node, char *data, void *msg_data)
{
	uint32_t status = 0, time_to_sleep = 0;
	char *value = NULL;

	if (get_value_from_map(data, "value", &value) == CC_RESULT_SUCCESS) {
		if (decode_uint_from_map(value, "status", &status) == CC_RESULT_SUCCESS) {
			if (status == 200) {
				if (calvinsys_timer_get_next_timeout(node, &time_to_sleep) == CC_RESULT_SUCCESS) {
					if (time_to_sleep <= CC_INACTIVITY_TIMEOUT) {
						cc_log("Sleep requested but timer about to trigger");
						return CC_RESULT_SUCCESS;
					}
				} else
					time_to_sleep = CC_SLEEP_TIME;
				cc_log("Enterring sleep");
#ifdef CC_TRACK_TIME
				node->seconds_of_sleep = time_to_sleep;
#endif
#ifdef CC_STORAGE_ENABLED
				node_set_state(node);
#else
				// TODO: Migrate actors before enterring sleep
#endif
				if (node->transport_client != NULL)
					node->transport_client->disconnect(node, node->transport_client);
				node->state = NODE_STOP;
				platform_deepsleep(node, time_to_sleep);
			} else
				cc_log_error("Failed to request sleep");
			return CC_RESULT_SUCCESS;
		}
	}

	cc_log_error("Failed to decode SLEEP_REQUEST reply");
	return CC_RESULT_FAIL;
}
#endif

result_t node_handle_token(port_t *port, const char *data, const size_t size, uint32_t sequencenbr)
{
	char *buffer = NULL;

	if (port->actor->state == ACTOR_ENABLED) {
		if (fifo_slots_available(port->fifo, 1)) {
			if (platform_mem_alloc((void **)&buffer, size) != CC_RESULT_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				return CC_RESULT_FAIL;
			}
			memcpy(buffer, data, size);
			if (fifo_com_write(port->fifo, buffer, size, sequencenbr) == CC_RESULT_SUCCESS)
				return CC_RESULT_SUCCESS;
			cc_log_error("Failed to write to fifo");
			platform_mem_free((void *)buffer);
		} else
			cc_log("Token received but no slots available");
	} else
		cc_log("Token received but actor not enabled");

	return CC_RESULT_FAIL;
}

void node_handle_token_reply(node_t *node, char *port_id, uint32_t port_id_len, port_reply_type_t reply_type, uint32_t sequencenbr)
{
	port_t *port = port_get(node, port_id, port_id_len);

	if (port != NULL) {
		if (reply_type == PORT_REPLY_TYPE_ACK)
			fifo_com_commit_read(port->fifo, sequencenbr);
		else if (reply_type == PORT_REPLY_TYPE_NACK)
			fifo_com_cancel_read(port->fifo, sequencenbr);
		else if (reply_type == PORT_REPLY_TYPE_ABORT)
			cc_log_debug("TODO: handle ABORT");
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

	cc_log_error("Failed to handle message");
	return CC_RESULT_FAIL;
}

static result_t node_setup(node_t *node)
{
#ifdef CC_TLS_ENABLED
	char name[50];
	char domain[50];

	return crypto_get_node_info(domain, name, node->id);
#endif

#ifdef CC_STORAGE_ENABLED
	if (node_get_state(node) == CC_RESULT_SUCCESS)
		return CC_RESULT_SUCCESS;
#endif

	gen_uuid(node->id, NULL);

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
			cc_log_error("Failed to create proxy link");
			return CC_RESULT_FAIL;
		}
	}

	if (node->storage_tunnel == NULL) {
		node->storage_tunnel = tunnel_create(node, TUNNEL_TYPE_STORAGE, TUNNEL_DISCONNECTED, peer_id, peer_id_len, NULL, 0);
		if (node->storage_tunnel == NULL) {
			cc_log_error("Failed to create storage tunnel");
			return CC_RESULT_FAIL;
		}
		tunnel_add_ref(node->storage_tunnel);
	}

	if (proto_send_node_setup(node, node_setup_reply_handler) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	while (node->state != NODE_STARTED && node->state != NODE_STOP)
		platform_evt_wait(node, 0);

	if (node->state != NODE_STARTED) {
		cc_log_error("Failed connect to proxy");
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

result_t node_init(node_t *node, const char *attributes, const char *proxy_uris, const char *storage_dir)
{
	char *uris = (char *)proxy_uris, *uri = NULL;
	list_t *item = NULL;

	node->state = NODE_DO_START;
	node->fire_actors = fire_actors;
	node->transport_client = NULL;
	node->proxy_link = NULL;
	node->platform = NULL;
	node->links = NULL;
	node->storage_tunnel = NULL;
	node->tunnels = NULL;
	node->actors = NULL;
#ifdef CC_TRACK_TIME
	node->seconds = 0;
	node->seconds_of_sleep = 0;
#endif

	if (attributes != NULL) {
		if (platform_mem_alloc((void **)&node->attributes, strlen(attributes) + 1) != CC_RESULT_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_RESULT_FAIL;
		}
		strcpy(node->attributes, attributes);
	}

	if (storage_dir != NULL) {
		if (platform_mem_alloc((void **)&node->storage_dir, strlen(storage_dir) + 1) != CC_RESULT_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_RESULT_FAIL;
		}
		strcpy(node->storage_dir, storage_dir);
	}

	if (platform_create(node) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to create platform object");
		return CC_RESULT_FAIL;
	}

	if (platform_mem_alloc((void **)&node->calvinsys, sizeof(calvinsys_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	node->calvinsys->node = node;
	node->calvinsys->capabilities = NULL;
	node->calvinsys->handlers = NULL;
	node->calvinsys->next_id = 1;
	node->actor_types = NULL;

	if (platform_create_calvinsys(&node->calvinsys) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to create calvinsys");
		return CC_RESULT_FAIL;
	}

	if (calvinsys_timer_create(&node->calvinsys) != CC_RESULT_SUCCESS)
		cc_log_error("Failed to create calvinsys 'timer'");

	if (actor_store_init(&node->actor_types) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to create actor types");
		return CC_RESULT_FAIL;
	}

	if (node_setup(node) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to setup runtime");
		return CC_RESULT_FAIL;
	}

	if (node->proxy_uris == NULL && uris != NULL) {
		uri = strtok(uris, " ");
		while (uri != NULL) {
			list_add_n(&node->proxy_uris, uri, strlen(uri), NULL, 0);
			uri = strtok(NULL, " ");
		}
	}

	cc_log("Runtime initialized");
	cc_log("----------------------------------------");
	cc_log("ID: %s", node->id);
	if (node->attributes != NULL)
		cc_log("Attributes: %s", node->attributes);
	if (node->proxy_uris != NULL) {
		cc_log("Proxy URIs:");
		item = node->proxy_uris;
		while (item != NULL) {
			cc_log(" %s", item->id);
			item = item->next;
		}
	}
	if (node->calvinsys != NULL) {
		cc_log("Capabilities:");
		item = node->calvinsys->capabilities;
		while (item != NULL) {
			cc_log(" %s", item->id);
			item = item->next;
		}
	}
	if (node->actor_types != NULL) {
		cc_log("Actor types:");
		item = node->actor_types;
		while (item != NULL) {
			cc_log(" %s", item->id);
			item = item->next;
		}
	}
	if (node->storage_dir != NULL)
		cc_log("Storage directory: %s", node->storage_dir);
#ifdef CC_DEEPSLEEP_ENABLED
	cc_log("Inactivity timeout: %d seconds", CC_INACTIVITY_TIMEOUT);
	cc_log("Sleep time: %d seconds", CC_SLEEP_TIME);
#endif
	if (node->actors != NULL) {
		cc_log("Actors:");
		item = node->actors;
		while (item != NULL) {
			cc_log(" %s %s", ((actor_t *)item->data)->name, item->id);
			item = item->next;
		}
	}
#ifdef CC_TRACK_TIME
	cc_log("Time: '%ld's", node->seconds);
#endif
	cc_log("----------------------------------------");

	return CC_RESULT_SUCCESS;
}

static void node_free(node_t *node)
{
	list_t *item = NULL, *tmp_item = NULL;
	calvinsys_handler_t *handler = NULL;
	calvinsys_capability_t *capability = NULL;

	item = node->proxy_uris;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		platform_mem_free((void *)tmp_item->id);
		platform_mem_free((void *)tmp_item);
	}

	item = node->actor_types;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		platform_mem_free((void *)tmp_item->id);
		platform_mem_free((void *)tmp_item->data);
		platform_mem_free((void *)tmp_item);
	}

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

	if (node->platform != NULL)
		platform_mem_free((void *)node->platform);

	item = node->calvinsys->capabilities;
	while (item != NULL) {
		tmp_item = item;
		item = item->next;
		capability = (calvinsys_capability_t *)tmp_item->data;
		if (capability->state != NULL)
			platform_mem_free((void *)capability->state);
		platform_mem_free((void *)tmp_item->id);
		platform_mem_free((void *)tmp_item->data);
		platform_mem_free((void *)tmp_item);
	}

	while (node->calvinsys->handlers != NULL) {
		handler = node->calvinsys->handlers;
		node->calvinsys->handlers = node->calvinsys->handlers->next;
		calvinsys_delete_handler(handler);
	}

	platform_mem_free((void *)node->calvinsys);

	if (node->attributes != NULL)
		platform_mem_free((void *)node->attributes);

	if (node->storage_dir != NULL)
		platform_mem_free((void *)node->storage_dir);

	platform_mem_free((void *)node);
}

result_t node_run(node_t *node)
{
	list_t *item = NULL;
#ifdef CC_DEEPSLEEP_ENABLED
	uint32_t next_timer_timeout = 0;
#endif

	if (node->fire_actors == NULL) {
		cc_log_error("No actor scheduler set");
		return CC_RESULT_FAIL;
	}

	while (node->state != NODE_STOP) {
		item = node->proxy_uris;
		while (item != NULL && node->state != NODE_STOP) {
			node->state = NODE_DO_START;
			if (node_connect_to_proxy(node, item->id) == CC_RESULT_SUCCESS) {
				while (node->state != NODE_STOP && node->transport_client->state == TRANSPORT_ENABLED) {
					if (node->state != NODE_STARTED || node->actors == NULL) {
						platform_evt_wait(node, CC_INDEFINITELY_TIMEOUT);
						continue;
					}

					if (node->fire_actors(node))
						continue;

					if (!platform_evt_wait(node, CC_INACTIVITY_TIMEOUT)) {
#ifdef CC_DEEPSLEEP_ENABLED
						if (calvinsys_timer_get_next_timeout(node, &next_timer_timeout) == CC_RESULT_SUCCESS) {
							if (next_timer_timeout > CC_INACTIVITY_TIMEOUT) {
								cc_log("Requesting sleep for %ld seconds", next_timer_timeout);
								if (proto_send_sleep_request(node, next_timer_timeout, node_enter_sleep_reply_handler) != CC_RESULT_SUCCESS)
									cc_log_error("Failed send sleep request");
							}
						} else {
							// no timers active
							cc_log("Requesting sleep for %d seconds", CC_SLEEP_TIME);
							if (proto_send_sleep_request(node, CC_SLEEP_TIME, node_enter_sleep_reply_handler) != CC_RESULT_SUCCESS)
								cc_log_error("Failed send sleep request");
						}
#endif
					}
				}
			}

			if (node->transport_client != NULL) {
				node->transport_client->disconnect(node, node->transport_client);
				node->transport_client->free(node->transport_client);
				node->transport_client = NULL;
			}
			item = item->next;
		}
		platform_evt_wait(node, CC_RECONNECT_TIMEOUT);
	}

	platform_stop(node);

	node_free(node);

	cc_log("Runtime stopped");
	return CC_RESULT_SUCCESS;
}
