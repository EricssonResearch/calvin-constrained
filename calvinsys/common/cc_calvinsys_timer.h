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
#ifndef CC_TIMER_H
#define CC_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "../../runtime/north/cc_common.h"
#include "../cc_calvinsys.h"

typedef struct calvinsys_timer_t {
	uint32_t timeout;
  uint32_t last_triggered;
	bool active;
} calvinsys_timer_t;

result_t calvinsys_timer_create(calvinsys_t **calvinsys);
result_t calvinsys_timer_get_next_timeout(struct node_t *node, uint32_t *time);

#endif /* CC_TIMER_H */
