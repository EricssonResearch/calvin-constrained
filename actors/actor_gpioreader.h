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
#ifndef ACTOR_GPIOREADER_H
#define ACTOR_GPIOREADER_H

#include "../common.h"
#include "../actor.h"
#include "../platform.h"

typedef struct state_gpioreader_t {
	calvin_ingpio_t *gpio;
	calvinsys_io_giohandler_t *gpiohandler;
} state_gpioreader_t;

result_t actor_gpioreader_init(actor_t **actor, list_t *attributes);
result_t actor_gpioreader_set_state(actor_t **actor, list_t *attributes);
bool actor_gpioreader_fire(actor_t *actor);
void actor_gpioreader_free(actor_t *actor);
result_t actor_gpioreader_get_managed_attributes(actor_t *actor, list_t **attributes);

#endif /* ACTOR_GPIOREADER_H */
