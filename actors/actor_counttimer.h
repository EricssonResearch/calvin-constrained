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
#ifndef ACTOR_COUNTTIMER_H
#define ACTOR_COUNTTIMER_H

#include "../common.h"
#include "../actor.h"
#include "../platform.h"

typedef struct state_count_timer_t {
	uint32_t count;
	uint32_t sleep;
	uint32_t steps;
	calvin_timer_t *timer;
} state_count_timer_t;

result_t actor_count_timer_init(char *obj_actor_state, actor_state_t **state);
result_t actor_count_timer(actor_t *actor);
void free_count_timer_state(actor_t *actor);
char *serialize_count_timer(actor_state_t *state, char **buffer);

#endif /* ACTOR_COUNTTIMER_H */