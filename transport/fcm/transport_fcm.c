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
#include <string.h>
#include "../../platform.h"
#include "../../node.h"
#include "transport_fcm.h"
#include "../../api.h"

#include <sys/socket.h>
#include "../../platform/android/platform_android.h"

/*
 * This transport file is only used for pure calvin data messages,
 * platform commands are handled elsewhere.
 */

static result_t transport_fcm_handle_connect_reply(node_t *node, char *buffer, size_t len)
{
	char cmd[3];

	// get command
	memset(cmd, 0, 3);
	memcpy(cmd, buffer + 4, 2);

	if (strcmp(cmd, CONNECT_REPLY) == 0) {
		// TODO: Always success? Include and parse result in payload
		log("FCM transport connected");
		return SUCCESS;
	}

	return FAIL;
}

static result_t transport_fcm_connect(node_t *node, transport_client_t *transport_client)
{
	char buffer[TRANSPORT_RX_BUFFER_SIZE];
	int len = strlen(transport_client->uri);

	transport_set_length_prefix(buffer, len + 2);
	memcpy(buffer + 6, transport_client->uri, strlen(transport_client->uri));

	if (((android_platform_t *)node->platform)->send_upstream_platform_message(transport_client, RUNTIME_CALVIN_MSG, buffer, len + 6) != SUCCESS) {
		log_error("Failed to send connect request");
		return FAIL;
	}

	return transport_handle_data(node, transport_client, transport_fcm_handle_connect_reply);
}

static int transport_fcm_send(transport_client_t *transport_client, char *data, size_t size)
{
    transport_fcm_client_t *fcm_client = (transport_fcm_client_t *)transport_client->client_state;

	return ((android_platform_t *)fcm_client->node->platform)->send_upstream_platform_message(transport_client, RUNTIME_CALVIN_MSG, data, size);
}

static int transport_fcm_recv(transport_client_t *transport_client, char *buffer, size_t size)
{
    transport_fcm_client_t *fcm_client = (transport_fcm_client_t *)transport_client->client_state;

	return read(((android_platform_t*)fcm_client->node->platform)->downstream_platform_fd[0], buffer, size);
}

static void transport_fcm_disconnect(node_t *node, transport_client_t *transport_client)
{
	// TODO: Send disconnect message to calvin base
	// TODO: Close these pipes somewhere (from java)
	/*
	close(((android_platform_t *) node->platform)->downstream_platform_fd[0]);
	close(((android_platform_t *) node->platform)->downstream_platform_fd[1]);
	close(((android_platform_t *) node->platform)->upstream_platform_fd[0]);
	close(((android_platform_t *) node->platform)->downstream_platform_fd[1]);
	 */
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
	transport_fcm_client_t *fcm_client = NULL;

	if (platform_mem_alloc((void **)&transport_client, sizeof(transport_client_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	memset(transport_client, 0, sizeof(transport_client_t));

	if (platform_mem_alloc((void **)&fcm_client, sizeof(transport_fcm_client_t)) != SUCCESS) {
		platform_mem_free((void *)transport_client);
		log_error("Failed to allocate memory");
		return NULL;
	}

	fcm_client->node = node;
	transport_client->client_state = fcm_client;
	transport_client->transport_type = TRANSPORT_FCM_TYPE;
	transport_client->rx_buffer.buffer = NULL;
	transport_client->rx_buffer.pos = 0;
	transport_client->rx_buffer.size = 0;
	transport_client->prefix_len = 6;

	transport_client->connect = transport_fcm_connect;
	transport_client->send = transport_fcm_send;
	transport_client->recv = transport_fcm_recv;
	transport_client->disconnect = transport_fcm_disconnect;
	transport_client->free = transport_fcm_free;

	// Dependency for Android, must be run on an Android phone as of now.
	transport_client->state = TRANSPORT_PENDING;

	return transport_client;
}
