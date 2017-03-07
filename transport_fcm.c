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
#include <unistd.h>
#include "platform.h"
#include "node.h"
#include "transport_fcm.h"
#include "api.h"

#ifdef PLATFORM_ANDROID
#include <sys/socket.h>
#include "platform/android/platform_android.h"
#endif
/*
 * This transport file is only used for pure calvin data messages,
 * platform commands are handled elsewhere.
 */

result_t send_fcm_connect_request(struct node_t* node, char* iface)
{
	result_t result = transport_create_tx_buffer(node->transport_client, BUFFER_SIZE);
	char* buf = node->transport_client->tx_buffer.buffer + 4;

	memcpy(buf, iface, strlen(iface)+1);
	android_platform_t* platform = ((android_platform_t*) node->platform);
	return platform->send_upstream_platform_message(node, FCM_CONNECT, node->transport_client, strlen(iface)+1);
}

static result_t transport_fcm_connect(node_t *node, transport_client_t *transport_client)
{
	send_fcm_connect_request(node, node->transport_client->uri);
	node->transport_client->state = TRANSPORT_PENDING;
	return SUCCESS;
}

static result_t transport_fcm_send_tx_buffer(const node_t* node, transport_client_t *transport_client, size_t size)
{
	android_platform_t* platform = (android_platform_t*) node->platform;
	return platform->send_upstream_platform_message(node, RUNTIME_CALVIN_MSG, transport_client, size);
}

static void transport_fcm_disconnect(node_t* node, transport_client_t *transport_client)
{
	// TODO: Send disconnect message to calvin base
	close(((android_platform_t*) node->platform)->downstream_platform_fd[0]);
	close(((android_platform_t*) node->platform)->downstream_platform_fd[1]);
	close(((android_platform_t*) node->platform)->upstream_platform_fd[0]);
	close(((android_platform_t*) node->platform)->downstream_platform_fd[1]);
}

static void transport_fcm_free(transport_client_t *transport_client)
{
	if (transport_client->client_state != NULL)
		platform_mem_free((void *)transport_client->client_state);
	platform_mem_free((void *)transport_client);
}

transport_client_t *transport_fcm_create(struct node_t *node, char *uri)
{
	transport_client_t *transport_client = NULL;

	if (platform_mem_alloc((void **)&transport_client, sizeof(transport_client_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	memset(transport_client, 0, sizeof(transport_client_t));

	transport_client->state = TRANSPORT_INTERFACE_DOWN;
	transport_client->rx_buffer.buffer = NULL;
	transport_client->rx_buffer.pos = 0;
	transport_client->rx_buffer.size = 0;
	transport_client->tx_buffer.buffer = NULL;
	transport_client->tx_buffer.pos = 0;
	transport_client->tx_buffer.size = 0;

	transport_client->connect = transport_fcm_connect;
	transport_client->send_tx_buffer = transport_fcm_send_tx_buffer;
	transport_client->disconnect = transport_fcm_disconnect;
	transport_client->free = transport_fcm_free;

	// Dependency for Android, must be run on an Android phone as of now.
	transport_client->state = TRANSPORT_PENDING;
	return transport_client;
}
