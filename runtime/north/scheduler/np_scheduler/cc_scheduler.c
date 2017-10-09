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
#include "../cc_scheduler.h"
#include "../../../../runtime/north/cc_actor.h"
#include "../../../../runtime/north/cc_common.h"

/** Default non-preemptive logging actor scheduler
 */
bool fire_actors(cc_node_t *node)
{
	bool fired = false;
	cc_list_t *actors = NULL, *ports = NULL;
	cc_actor_t *actor = NULL;

	actors = node->actors;
	while (actors != NULL) {
		actor = (cc_actor_t *)actors->data;
		if (actor->state == CC_ACTOR_DO_DELETE) {
			actors = actors->next;
			cc_actor_free(node, actor, true);
			continue;
		}

		if (actor->state == CC_ACTOR_ENABLED) {
			if (actor->fire(actor)) {
				cc_log("Scheduler: Fired '%s', time '%ld'", actor->id, cc_node_get_time(node));
				fired = true;
			}
		}

		// handle pending in-ports
		ports = actor->in_ports;
		while (ports != NULL) {
			cc_port_transmit(node, (cc_port_t *)ports->data);
			ports = ports->next;
		}

		// handle pending out-port and send tokens
		ports = actor->out_ports;
		while (ports != NULL) {
			cc_port_transmit(node, (cc_port_t *)ports->data);
			ports = ports->next;
		}
		actors = actors->next;
	}

	return fired;
}
