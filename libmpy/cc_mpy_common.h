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

#ifndef CC_MPY_COMMON_H
#define CC_MPY_COMMON_H

#if CC_USE_PYTHON
#include "runtime/north/cc_common.h"
#include "py/obj.h"

cc_result_t cc_mpy_decode_to_mpy_obj(char *buffer, mp_obj_t *value);
cc_result_t cc_mpy_encode_from_mpy_obj(mp_obj_t input, char **buffer, size_t *size, bool alloc);

#endif

#endif /* CC_MPY_COMMON_H */
