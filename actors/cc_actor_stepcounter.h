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
#ifndef ACTOR_STEPCOUNTER_H
#define ACTOR_STEPCOUNTER_H

#include "../runtime/north/cc_common.h"

typedef struct actor_stepcounter_state_t {
	calvinsys_obj_t *calvinsys_stepcounter;
	char *stepcount;
	size_t stepcount_size;
} actor_stepcounter_state_t;

result_t actor_stepcounter_register(list_t **actor_types);

#endif
