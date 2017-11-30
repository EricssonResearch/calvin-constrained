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
#ifndef CC_CALVINSYS_DS18B20_H
#define CC_CALVINSYS_DS18B20_H

#include "runtime/north/cc_common.h"
#include "calvinsys/cc_calvinsys.h"

typedef struct cc_calvinsys_temperature_state_t {
	uint8_t pin;
} cc_calvinsys_temperature_state_t;

cc_result_t cc_calvinsys_ds18b20_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs);

#endif /* CC_CALVINSYS_DS18B20_H */
