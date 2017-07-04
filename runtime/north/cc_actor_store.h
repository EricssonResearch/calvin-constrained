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
#ifndef ACTOR_STORE_H
#define ACTOR_STORE_H

#include <stdbool.h>
#include "cc_common.h"
#include "cc_actor.h"

typedef struct actor_type_t {
	result_t (*init)(actor_t **actor, list_t *attributes);
	result_t (*set_state)(actor_t **actor, list_t *attributes);
	void (*free_state)(actor_t *actor);
	bool (*fire_actor)(actor_t *actor);
	result_t (*get_managed_attributes)(actor_t *actor, list_t **attributes);
	void (*will_migrate)(actor_t *actor);
	void (*will_end)(actor_t *actor);
	void (*did_migrate)(actor_t *actor);
} actor_type_t;

result_t actor_store_init(list_t **actor_types);

#endif /* ACTOR_STORE_H */
