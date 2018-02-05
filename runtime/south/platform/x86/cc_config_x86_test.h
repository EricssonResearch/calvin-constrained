/*
 * Copyright (c) 2018 Ericsson AB
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

#ifndef CC_CONFIG_X86_TEST_H
#define CC_CONFIG_X86_TEST_H

#if CC_USE_PYTHON
#include "cc_config_x86_mpy.h"
#ifndef _CC_C_ACTORS
#define CC_C_ACTORS
#endif
#else
#include "cc_config_x86.h"
#endif

#undef CC_CAPABILITIES
#define CC_CAPABILITIES _CC_CAPABILITIES, \
	{ "mock.shadow", NULL, NULL, NULL, NULL, false }

#undef CC_C_ACTORS
#define CC_C_ACTORS _CC_C_ACTORS, \
	{ "test.FakeShadow", cc_actor_identity_setup }

#endif