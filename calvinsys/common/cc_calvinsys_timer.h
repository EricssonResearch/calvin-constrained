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
#ifndef CC_CALVINSYS_TIMER_H
#define CC_CALVINSYS_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "runtime/north/cc_common.h"
#include "calvinsys/cc_calvinsys.h"

typedef struct cc_calvinsys_timer_t {
	uint32_t timeout;
	bool armed;
  uint32_t next_time;
	bool repeats;
	bool triggered;
} cc_calvinsys_timer_t;

/**
 * cc_calvinsys_timer_create() - Create calvinsys timer objects
 * @calvinsys Calvinsys object
 */

cc_result_t cc_calvinsys_timer_create(cc_calvinsys_t **calvinsys);
/**
 * cc_calvinsys_timers_check() - Updates timers and gets next timeout
 * @node the node object
 * @timeout timeout value for next timer
 */
void cc_calvinsys_timers_check(struct cc_node_t *node, uint32_t *timeout);

#endif /* CC_CALVINSYS_TIMER_H */
