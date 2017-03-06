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

#include "common.h"
#include "node.h"

#define RUNTIME_STOP "RS"
#define RUNTIME_CALVIN_MSG "CM"
#define RUNTIME_STARTED "RR"
#define FCM_CONNECT "FC"
#define CONNECT_REPLY "FR"

result_t api_runtime_stop();
result_t* api_runtime_init();
result_t api_runtime_start(char* name, char* capabilities, char* proxy_uris);
result_t api_send_downstream_platform_message(char* cmd, transport_client_t* tc, char* data, size_t data_size);
result_t api_send_upstream_calvin_message(char* cmd, transport_client_t* tc, size_t data_size);
result_t api_read_upstream(node_t* node, char* buffer, size_t size);
result_t api_write_downstream_calvin_payload(node_t* node, char* payload, size_t size);