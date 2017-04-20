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
#include "scheduler.h"
#include "actor.h"
#include "common.h"

/** Default non-preemptive actor scheduler
 */
bool fire_actors(node_t *node)
{
  bool fired = false;
 	list_t *tmp_list = NULL;
 	actor_t *actor = NULL;

 	tmp_list = node->actors;
 	while (tmp_list != NULL) {
 		actor = (actor_t *)tmp_list->data;
 		if (actor->state == ACTOR_ENABLED) {
 			if (actor->fire(actor)) {
 				log("Fired '%s'", actor->name);
 				fired = true;
 			}
 		}
 		tmp_list = tmp_list->next;
 	}

 	return fired;
}
