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

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeInit(JNIEnv* env, jobject this)
{
    api_runtime_init();
}

JNIEXPORT jbyteArray JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_readUpstreamData(JNIEnv* env, jobject this)
{
    char buffer[BUFFER_SIZE];
    memset(&buffer, 0, BUFFER_SIZE);
    api_read_upstream(get_node(), buffer, BUFFER_SIZE);
    jbyteArray data;
    size_t size = get_message_len(buffer);

    data = (*env)->NewByteArray(env, size+4);
    (*env)->SetByteArrayRegion(env, data, 0, size+4, buffer);
    return data;
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeStart(JNIEnv* env, jobject this)
{
    //char* proxy_uris = "calvinip://192.168.0.108:5000";
    char* proxy_uris = "calvinfcm://hej:san";
    char* capabilities = "[1000, 1001, 1002]";
    char* name = "Calvin Android";
    api_runtime_start(name, capabilities, proxy_uris);
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeStop(JNIEnv* env, jobject this)
{
    api_runtime_stop();
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeCalvinPayload(JNIEnv* env, jobject this, jbyteArray data)
{
    int len = (*env)->GetArrayLength(env, data);
    char payload_data[len];
    // transport_append_buffer_prefix(payload_data, len);
    memset(payload_data, 0, len);
    (*env)->GetByteArrayRegion(env, data, 0, len, payload_data);
    api_write_downstream_calvin_payload(get_node(), payload_data, len);
}

JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_fcmTransportConnected(JNIEnv* env, jobject this) {
    node_t* node = get_node();
    char d[4] = {0, 0, 0, 0};
    api_send_downstream_platform_message(CONNECT_REPLY, node->transport_client, d, 0);
}
