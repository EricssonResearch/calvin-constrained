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
#include <string.h>
#include <jni.h>
#include <stdio.h>
#include <android/log.h>
#include <unistd.h>
#include "cc_jni_api.h"
#include "cc_platform_android.h"
#include "../cc_platform.h"
#include "../../../../cc_api.h"
#include "../../../north/cc_transport.h"

jlong get_jlong_from_pointer(void *ptr)
{
	long long_ptr = (long)ptr;
	return long_ptr;
}

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
void *get_ptr_from_jlong(jlong ptr_value)
{
	void *ptr = (void *)ptr_value;
	return ptr;
}

JNIEXPORT jlong JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeInit(JNIEnv *env, jobject this, jstring j_proxy_uris, jstring j_attributes, jstring j_storage_dir)
{
	cc_node_t *node = NULL;
	char *storage_dir = (char *)(*env)->GetStringUTFChars(env, j_storage_dir, 0);
	char *proxy_uris = (char *)(*env)->GetStringUTFChars(env, j_proxy_uris, 0);
	char *attributes = (char *)(*env)->GetStringUTFChars(env, j_attributes, 0);

	cc_api_runtime_init(&node, attributes, proxy_uris, storage_dir);

	return get_jlong_from_pointer(node);
}

JNIEXPORT jbyteArray JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_readUpstreamData(JNIEnv *env, jobject this, jlong jnode)
{
	char buffer[CC_TRANSPORT_RX_BUFFER_SIZE];
	jbyteArray data = NULL;
	cc_node_t *node = (cc_node_t *)get_ptr_from_jlong(jnode);
	android_platform_t *platform = (android_platform_t *)node->platform;
	size_t size;

	memset(&buffer, 0, CC_TRANSPORT_RX_BUFFER_SIZE);
	if (platform->read_upstream(node, buffer, CC_TRANSPORT_RX_BUFFER_SIZE) == CC_SUCCESS) {
		size = cc_transport_get_message_len(buffer);
		data = (*env)->NewByteArray(env, size + 7);
		(*env)->SetByteArrayRegion(env, data, 0, size + 7, buffer);
	}

	return data;
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeStart(JNIEnv *env, jobject this, jlong jnode)
{
	cc_api_runtime_start((cc_node_t *)get_ptr_from_jlong(jnode));
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeStop(JNIEnv *env, jobject this, jlong node)
{
	cc_api_runtime_stop((cc_node_t *)get_ptr_from_jlong(node));
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeCalvinPayload(JNIEnv *env, jobject this, jbyteArray data, jlong jnode)
{
	// assumes data contains a complete calvin message
	int len = (*env)->GetArrayLength(env, data);
	char payload_data[CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE + len];
	cc_node_t *node = (cc_node_t *)get_ptr_from_jlong(jnode);
	android_platform_t *platform = (android_platform_t *)node->platform;

	cc_transport_set_length_prefix(payload_data, PLATFORM_ANDROID_COMMAND_SIZE + len);
	memcpy(payload_data + CC_TRANSPORT_LEN_PREFIX_SIZE, PLATFORM_ANDROID_RUNTIME_CALVIN_MSG, PLATFORM_ANDROID_COMMAND_SIZE);
	(*env)->GetByteArrayRegion(env, data, 0, len, payload_data + CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE);

	platform->send_downstream_platform_message(node, payload_data, len + CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE);
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_fcmTransportConnected(JNIEnv *env, jobject this, jlong node_p)
{
	cc_node_t *node = (cc_node_t *)get_ptr_from_jlong(node_p);
	char payload_data[CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE];

	cc_transport_set_length_prefix(payload_data, PLATFORM_ANDROID_COMMAND_SIZE);
	memcpy(payload_data + CC_TRANSPORT_LEN_PREFIX_SIZE, PLATFORM_ANDROID_CONNECT_REPLY, PLATFORM_ANDROID_COMMAND_SIZE);
	((android_platform_t *)node->platform)->send_downstream_platform_message(node, payload_data, CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE);
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeSerializeAndStop(JNIEnv *env, jobject this, jlong node_p)
{
	cc_node_t *node = (cc_node_t *)get_ptr_from_jlong(node_p);
	android_platform_t *platform = (android_platform_t *)node->platform;
	char payload_data[CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE];

	cc_transport_set_length_prefix(payload_data, PLATFORM_ANDROID_COMMAND_SIZE);
	memcpy(payload_data + CC_TRANSPORT_LEN_PREFIX_SIZE, PLATFORM_ANDROID_RUNTIME_SERIALIZE_AND_STOP, PLATFORM_ANDROID_COMMAND_SIZE);
	platform->send_downstream_platform_message(node, payload_data, CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE);
}

JNIEXPORT jint JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_getNodeState(JNIEnv *env, jobject this, jlong node_p)
{
	cc_node_t *node = (cc_node_t *)get_ptr_from_jlong(node_p);

	if (node == NULL)
		return (jint) 3;
	switch (node->state) {
	case CC_NODE_DO_START:
		return (jint) 0;
	case CC_NODE_PENDING:
		return (jint) 1;
	case CC_NODE_STARTED:
		return (jint) 2;
	case CC_NODE_STOP:
		return (jint) 3;
	}
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_clearSerialization(JNIEnv *env, jobject this, jstring j_filedir)
{
	char *filedir = (char *)(*env)->GetStringUTFChars(env, j_filedir, 0);

	cc_api_clear_serialization_file(filedir);
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_triggerConnectivityChange(JNIEnv *env, jobject this, jlong node_p)
{
	cc_node_t *node = (cc_node_t *)get_ptr_from_jlong(node_p);
	android_platform_t *platform = (android_platform_t *)node->platform;
	char payload_data[CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE];

	cc_transport_set_length_prefix(payload_data, PLATFORM_ANDROID_COMMAND_SIZE);
	memcpy(payload_data + CC_TRANSPORT_LEN_PREFIX_SIZE, PLATFORM_ANDROID_RUNTIME_TRIGGER_RECONNECT, PLATFORM_ANDROID_COMMAND_SIZE);
	platform->send_downstream_platform_message(node, payload_data, CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE);
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_registerExternalCalvinsys(JNIEnv *env, jobject this, jlong node_p, jstring j_name)
{
	cc_node_t *node = (cc_node_t *)get_ptr_from_jlong(node_p);
	android_platform_t *platform = (android_platform_t *)node->platform;
	char *name = (char *)(*env)->GetStringUTFChars(env, j_name, 0);
	int len = strlen(name);
	char payload_data[CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE + len];

	cc_transport_set_length_prefix(payload_data, PLATFORM_ANDROID_COMMAND_SIZE + len);
	memcpy(payload_data + CC_TRANSPORT_LEN_PREFIX_SIZE, PLATFORM_ANDROID_REGISTER_EXTERNAL_CALVINSYS, PLATFORM_ANDROID_COMMAND_SIZE);
	memcpy(payload_data + CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE, name, len);
	platform->send_downstream_platform_message(node, payload_data, CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE + len);
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_writeCalvinsysPayload(JNIEnv *env, jobject this, jbyteArray data, jlong jnode)
{
	cc_node_t *node = (cc_node_t *)get_ptr_from_jlong(jnode);
	android_platform_t *platform = (android_platform_t *)node->platform;
	int len = (*env)->GetArrayLength(env, data);
	char payload_data[CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE + len];

	cc_transport_set_length_prefix(payload_data, PLATFORM_ANDROID_COMMAND_SIZE + len);
	memcpy(payload_data + CC_TRANSPORT_LEN_PREFIX_SIZE, PLATFORM_ANDROID_EXTERNAL_CALVINSYS_PAYLOAD, PLATFORM_ANDROID_COMMAND_SIZE);
	(*env)->GetByteArrayRegion(env, data, 0, len, payload_data + (CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE));
	platform->send_downstream_platform_message(node, payload_data, CC_TRANSPORT_LEN_PREFIX_SIZE + PLATFORM_ANDROID_COMMAND_SIZE + len);
}
