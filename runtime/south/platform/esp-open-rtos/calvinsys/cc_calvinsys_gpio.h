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
#ifndef CALVINSYS_GPIO_H
#define CALVINSYS_GPIO_H

#include "../../../../../runtime/north/cc_common.h"
#include "../../../../../calvinsys/cc_calvinsys.h"

typedef enum {
	CC_GPIO_IN,
	CC_GPIO_OUT
} calvinsys_gpio_direction;

typedef struct calvinsys_gpio_state_t {
	uint8_t pin;
	calvinsys_gpio_direction direction;
} calvinsys_gpio_state_t;

calvinsys_handler_t *calvinsys_gpio_create_handler(calvinsys_t **calvinsys);

#endif /* CALVINSYS_GPIO_H */
