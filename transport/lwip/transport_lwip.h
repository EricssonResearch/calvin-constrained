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
#ifndef TRANSPORT_LWIP_H
#define TRANSPORT_LWIP_H

#include "lwip/tcp.h"

struct node_t;

typedef struct transport_lwip_client_t {
  char mac[40];
  bool has_pending_tx;
	struct tcp_pcb *tcp_port;
	struct node_t *node;
} transport_lwip_client_t;

transport_client_t *transport_lwip_get_client(void);

#endif /* TRANSPORT_LWIP_H */
