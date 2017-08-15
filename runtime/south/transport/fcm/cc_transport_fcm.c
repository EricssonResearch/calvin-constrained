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
#include <sys/socket.h>
#include "../../platform/cc_platform.h"
#include "../../../north/cc_node.h"
#include "cc_transport_fcm.h"
#include "../../../../cc_api.h"
#include "../../platform/android/cc_platform_android.h"

/*
 * This transport file is only used for pure calvin data messages,
 * platform commands are handled elsewhere.
 */

static result_t transport_fcm_connect(node_t *node, transport_client_t *transport_client)
{
	char buffer[TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE];

	if (((android_platform_t *)node->platform)->send_upstream_platform_message(
			node,
			PLATFORM_ANDROID_FCM_CONNECT,
			buffer,
			TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE) > 0) {
		transport_client->state = TRANSPORT_PENDING;
		return CC_RESULT_SUCCESS;
	}

	cc_log_error("Failed to send connect request");
	transport_client->state = TRANSPORT_INTERFACE_DOWN;

	return CC_RESULT_FAIL;
}

static int transport_fcm_send(transport_client_t *transport_client, char *data, size_t size)
{
	transport_fcm_client_t *fcm_client = (transport_fcm_client_t *)transport_client->client_state;

	cc_log("transport_fcm_send");

	return ((android_platform_t *)fcm_client->node->platform)->send_upstream_platform_message(
			fcm_client->node,
		PLATFORM_ANDROID_RUNTIME_CALVIN_MSG,
		data,
		size);
}

static int transport_fcm_recv(transport_client_t *transport_client, char *buffer, size_t size)
{
	transport_fcm_client_t *fcm_client = (transport_fcm_client_t *)transport_client->client_state;

	cc_log("transport_fcm_recv");
	return read(((android_platform_t *)fcm_client->node->platform)->downstream_platform_fd[0], buffer, size);
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

	if (platform_mem_alloc((void **)&transport_client, sizeof(transport_client_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	memset(transport_client, 0, sizeof(transport_client_t));

	if (platform_mem_alloc((void **)&fcm_client, sizeof(transport_fcm_client_t)) != CC_RESULT_SUCCESS) {
		platform_mem_free((void *)transport_client);
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	fcm_client->node = node;
	transport_client->client_state = fcm_client;
	transport_client->transport_type = TRANSPORT_FCM_TYPE;
	transport_client->rx_buffer.buffer = NULL;
	transport_client->rx_buffer.pos = 0;
	transport_client->rx_buffer.size = 0;
	transport_client->prefix_len = TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE;

	transport_client->connect = transport_fcm_connect;
	transport_client->send = transport_fcm_send;
	transport_client->recv = transport_fcm_recv;
	transport_client->disconnect = transport_fcm_disconnect;
	transport_client->free = transport_fcm_free;
	transport_client->state = TRANSPORT_INTERFACE_UP;
	strncpy(transport_client->uri, uri, strlen(uri) + 1);

	// Dependency for Android, must be run on an Android phone as of now.

	return transport_client;
}
