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
#ifndef TRANSPORT_COMMON_H
#define TRANSPORT_COMMON_H

#include "common.h"
#include "transport.h"
#include "node.h"

void transport_handle_data(transport_client_t *transport_client, char *buffer, size_t size);
result_t transport_create_tx_buffer(transport_client_t *transport_client, size_t size);
void transport_free_tx_buffer(transport_client_t *transport_client);
void transport_append_buffer_prefix(char *buffer, size_t size);
void transport_join(node_t *node, transport_client_t *transport_client);


#endif /* TRANSPORT_COMMON_H */
