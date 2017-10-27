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
#ifndef CC_CALVINSYS_GPIO_H
#define CC_CALVINSYS_GPIO_H

#include "runtime/north/cc_common.h"
#include "calvinsys/cc_calvinsys.h"

typedef enum {
	CC_GPIO_IN,
	CC_GPIO_OUT
} cc_calvinsys_gpio_direction;

typedef struct cc_calvinsys_gpio_state_t {
	uint8_t pin;
	cc_calvinsys_gpio_direction direction;
} cc_calvinsys_gpio_state_t;

cc_result_t cc_calvinsys_gpio_create(cc_calvinsys_t **calvinsys, const char *name, cc_calvinsys_gpio_state_t *state);

#endif /* CC_CALVINSYS_GPIO_H */
