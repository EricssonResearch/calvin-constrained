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
#include "runtime/south/platform/cc_platform.h"
#include "runtime/north/cc_node.h"
#include "cc_transport_fcm.h"
#include "cc_api.h"
#include "runtime/south/platform/android/cc_platform_android.h"

/*
 * This transport file is only used for pure calvin data messages,
 * platform commands are handled elsewhere.
 */

static cc_result_t transport_fcm_connect(cc_node_t *node, cc_transport_client_t *transport_client)
{
	char buffer[CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE];

	if (((android_platform_t *)node->platform)->send_upstream_platform_message(
			node,
			PLATFORM_ANDROID_FCM_CONNECT,
			buffer,
			CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE) > 0) {
		transport_client->state = CC_TRANSPORT_PENDING;
		return CC_SUCCESS;
	}

	cc_log_error("Failed to send connect request");
	transport_client->state = CC_TRANSPORT_INTERFACE_DOWN;

	return CC_FAIL;
}

static int transport_fcm_send(cc_transport_client_t *transport_client, char *data, size_t size)
{
	transport_fcm_client_t *fcm_client = (transport_fcm_client_t *)transport_client->client_state;

	cc_log("transport_fcm_send");

	return ((android_platform_t *)fcm_client->node->platform)->send_upstream_platform_message(
			fcm_client->node,
		PLATFORM_ANDROID_RUNTIME_CALVIN_MSG,
		data,
		size);
}

static int transport_fcm_recv(cc_transport_client_t *transport_client, char *buffer, size_t size)
{
	transport_fcm_client_t *fcm_client = (transport_fcm_client_t *)transport_client->client_state;

	cc_log("transport_fcm_recv");
	return read(((android_platform_t *)fcm_client->node->platform)->downstream_platform_fd[0], buffer, size);
}

static void transport_fcm_disconnect(cc_node_t *node, cc_transport_client_t *transport_client)
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

static void transport_fcm_free(cc_transport_client_t *transport_client)
{
	if (transport_client->client_state != NULL)
		cc_platform_mem_free((void *)transport_client->client_state);
	cc_platform_mem_free((void *)transport_client);
}

cc_transport_client_t *transport_fcm_create(struct cc_node_t *node, char *uri)
{
	cc_transport_client_t *transport_client = NULL;
	transport_fcm_client_t *fcm_client = NULL;

	if (cc_platform_mem_alloc((void **)&transport_client, sizeof(cc_transport_client_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	memset(transport_client, 0, sizeof(cc_transport_client_t));

	if (cc_platform_mem_alloc((void **)&fcm_client, sizeof(transport_fcm_client_t)) != CC_SUCCESS) {
		cc_platform_mem_free((void *)transport_client);
		cc_log_error("Failed to allocate memory");
		return NULL;
	}

	fcm_client->node = node;
	transport_client->client_state = fcm_client;
	transport_client->transport_type = CC_TRANSPORT_FCM_TYPE;
	transport_client->rx_buffer.buffer = NULL;
	transport_client->rx_buffer.pos = 0;
	transport_client->rx_buffer.size = 0;
	transport_client->prefix_len = CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE;

	transport_client->connect = transport_fcm_connect;
	transport_client->send = transport_fcm_send;
	transport_client->recv = transport_fcm_recv;
	transport_client->disconnect = transport_fcm_disconnect;
	transport_client->free = transport_fcm_free;
	transport_client->state = CC_TRANSPORT_INTERFACE_UP;
	strncpy(transport_client->uri, uri, strlen(uri) + 1);

	// Dependency for Android, must be run on an Android phone as of now.

	return transport_client;
}
