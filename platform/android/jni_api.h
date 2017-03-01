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

JNIEXPORT jstring JNICALL Java_ericsson_com_calvin_calvin_1constrained_CCActivity_stringFromJNI(JNIEnv*, jobject);

JNIEXPORT jstring JNICALL Java_ericsson_com_calvin_calvin_1constrained_CCActivity_platformStart(JNIEnv*, jobject);
JNIEXPORT jbyteArray JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_readUpstreamData(JNIEnv* env, jobject this);
JNIEXPORT void JNICALL Java_ericsson_com_calvin_calvin_1constrained_Calvin_runtimeCalvinPayload(JNIEnv* env, jobject this, jstring data);