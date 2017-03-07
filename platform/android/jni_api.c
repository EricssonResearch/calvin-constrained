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
#include "platform.h"
#include "api.h"
#include <android/log.h>
#include <unistd.h>
#include "platform_android.h"

jlong get_jlong_from_pointer(void* ptr)
{
	long long_ptr = (long)ptr;
	return long_ptr;
}

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
void* get_ptr_from_jlong(jlong ptr_value)
{
	void* ptr = (void*) ptr_value;
	return ptr;
}

JNIEXPORT jlong JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeInit(JNIEnv* env, jobject this)
{
	node_t* node;

	api_runtime_init(&node);
	return get_jlong_from_pointer(node);
}

JNIEXPORT jbyteArray JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_readUpstreamData(JNIEnv* env, jobject this, jlong jnode)
{
	char buffer[BUFFER_SIZE];

	memset(&buffer, 0, BUFFER_SIZE);
	node_t* node = (node_t*)get_ptr_from_jlong(jnode);
	android_platform_t* platform = (android_platform_t*) node->platform;

	platform->read_upstream(node, buffer, BUFFER_SIZE);
	jbyteArray data;
	size_t size = get_message_len(buffer);

	data = (*env)->NewByteArray(env, size+4);
	(*env)->SetByteArrayRegion(env, data, 0, size+4, buffer);
	return data;
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeStart(JNIEnv* env, jobject this, jlong node, jstring j_proxy_uris, jstring j_name)
{
	char* proxy_uris = (char*) (*env)->GetStringUTFChars(env, j_proxy_uris, 0);
	char* name = (char*) (*env)->GetStringUTFChars(env, j_name, 0);

	api_runtime_start(name, proxy_uris, (node_t*)get_ptr_from_jlong(node));
	(*env)->ReleaseStringUTFChars(env, j_proxy_uris, proxy_uris);
	(*env)->ReleaseStringUTFChars(env, j_name, name);
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeStop(JNIEnv* env, jobject this, jlong node)
{
	api_runtime_stop((node_t*)get_ptr_from_jlong(node));
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeCalvinPayload(JNIEnv* env, jobject this, jbyteArray data, jlong jnode)
{
	int len = (*env)->GetArrayLength(env, data);
	char payload_data[len];

	memset(payload_data, 0, len);
	(*env)->GetByteArrayRegion(env, data, 0, len, payload_data);
	node_t* node = (node_t*)get_ptr_from_jlong(jnode);
	android_platform_t* platform = (android_platform_t*) node->platform;

	platform->send_downstream_platform_message(node, RUNTIME_CALVIN_MSG, node->transport_client, payload_data, len);
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_fcmTransportConnected(JNIEnv* env, jobject this, jlong node_p)
{
	node_t* node = (node_t*)get_ptr_from_jlong(node_p);
	char d[4] = {0, 0, 0, 0};

	((android_platform_t*) node->platform)->send_downstream_platform_message(node, CONNECT_REPLY, node->transport_client, d, 0);
}
