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
#ifndef ACTOR_GPIOWRITER_H
#define ACTOR_GPIOWRITER_H

#include "../runtime/north/cc_actor.h"
#include "../runtime/south/platform/cc_platform.h"

typedef struct state_gpiowriter_t {
	uint32_t pin;
	calvinsys_io_giohandler_t *gpiohandler;
} state_gpiowriter_t;

result_t actor_gpiowriter_init(actor_t **actor, list_t *attributes);
result_t actor_gpiowriter_set_state(actor_t **actor, list_t *attributes);
bool actor_gpiowriter_fire(actor_t *actor);
void actor_gpiowriter_free(actor_t *actor);
result_t actor_gpiowriter_get_managed_attributes(actor_t *actor, list_t **attributes);

#endif /* ACTOR_GPIOWRITER_H */
