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
#include "runtime/north/cc_common.h"

struct cc_calvinsys_obj_t;
struct cc_node_t;
struct cc_transport_client_t;
typedef int wint_t;
cc_result_t cc_calvinsys_temp_sensor_open(struct cc_calvinsys_obj_t *obj, cc_list_t *kwargs);
struct cc_transport_client_t *cc_transport_spritzer_create(struct cc_node_t *node, char *uri);

#define CC_USE_STORAGE (1)
#define CC_PYTHON_FLOATS (0)
#define CC_ACTOR_MODULES_DIR "/mnt/spif/mpys/"
#define CC_STATE_FILE "/mnt/spif/calvin.msgpack"

#define CC_CAPABILITIES \
	{ "io.temperature", cc_calvinsys_temp_sensor_open, NULL, NULL, NULL }

#define CC_TRANSPORTS \
	{ "calvinip", cc_transport_spritzer_create }
