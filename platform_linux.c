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
#include <stdlib.h>
#include "platform.h"

calvin_timer_t *create_recurring_timer(double interval)
{
	calvin_timer_t *calvin_timer = NULL;

	calvin_timer = (calvin_timer_t*)malloc(sizeof(calvin_timer_t));
	if (calvin_timer == NULL) {
		log_error("Failed to allocate memory");
		return NULL;
	}

	calvin_timer->interval = interval;
	time(&calvin_timer->last_triggered);

    return calvin_timer;
}

void stop_timer(calvin_timer_t *timer)
{
	if (timer != NULL) {
		free(timer);
	}
}

bool check_timer(calvin_timer_t *timer)
{
	time_t now;
	double diff;

	if (timer != NULL) {
		time(&now);
		diff = difftime(now, timer->last_triggered);
		if (diff > timer->interval) {
			timer->last_triggered = now;
			return true;
		}

	} else {
		log_error("timer is NULL");
	}

	return false;
}