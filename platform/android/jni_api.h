#include <jni.h>

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

JNIEXPORT jbyteArray JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_readUpstreamData(JNIEnv* env, jobject this, jlong jnode);
JNIEXPORT jlong JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeInit(JNIEnv* env, jobject this, jstring j_proxy_uris, jstring j_name, jstring j_storage_dir);
JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_fcmTransportConnected(JNIEnv* env, jobject this, jlong node);
JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeStop(JNIEnv* env, jobject this, jlong node);
JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeCalvinPayload(JNIEnv* env, jobject this, jbyteArray data, jlong jnode);
JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeStart(JNIEnv* env, jobject this, jlong jnode);
JNIEXPORT jint JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_getNodeState(JNIEnv* env, jobject this, jlong node_p);
JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_triggerConnectivityChange(JNIEnv* env, jobject this, jlong node_p);
JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_registerExternalCalvinsys(JNIEnv* env, jobject this, jlong node_p, jstring j_name);
JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_writeCalvinsysPayload(JNIEnv* env, jobject this, jbyteArray data, jlong jnode);
