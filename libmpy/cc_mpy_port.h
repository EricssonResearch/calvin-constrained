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

#ifndef CC_MPY_PORT_H
#define CC_MPY_PORT_H

#include <stdint.h>

bool cc_mpy_port_init(void *heap, uint32_t heap_size, uint32_t stack_size);
void cc_mpy_port_deinit();

#endif /* CC_MPY_PORT_H */
