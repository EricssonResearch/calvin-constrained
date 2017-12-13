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
#ifndef CC_API_H
#define CC_API_H

#include "runtime/north/cc_common.h"
#include "runtime/north/cc_node.h"

cc_result_t cc_api_runtime_init(cc_node_t **node, const char *attributes, const char *proxy_uris, const char *storage_dir);
cc_result_t cc_api_runtime_start(cc_node_t *node);
cc_result_t cc_api_runtime_stop(cc_node_t *node);
cc_result_t cc_api_runtime_serialize_and_stop(cc_node_t *node);
cc_result_t cc_api_reconnect(cc_node_t *node);
#if (CC_USE_STORAGE == 1)
cc_result_t cc_api_clear_serialization_file(char *filedir);
#endif
#endif /* CC_API_H */
