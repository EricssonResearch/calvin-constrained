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
#ifndef TRANSPORT_FCM_H
#define TRANSPORT_FCM_H

#include "../../../north/cc_transport.h"

struct cc_node_t;

typedef struct transport_fcm_client_t {
	struct cc_node_t *node;
} transport_fcm_client_t;

cc_transport_client_t *transport_fcm_create(struct cc_node_t *node, char *uri);

#endif /* TRANSPORT_FCM_H */
