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

#ifndef CC_MPY_CONFIGPORT_H
#define CC_MPY_CONFIGPORT_H

#include <stdint.h>
#include "../runtime/south/platform/cc_platform.h"

#define MICROPY_ENABLE_GC               (1)
#define MICROPY_PY_GC                   (1)
//#define MICROPY_PY_GC_COLLECT_RETVAL    (1)
#define MICROPY_ENABLE_COMPILER         (0)
#define MICROPY_ERROR_REPORTING         (MICROPY_ERROR_REPORTING_DETAILED)
#define MICROPY_ERROR_PRINTER           (&cc_log_error)
#define MICROPY_CPYTHON_COMPAT          (1)
#define MICROPY_MODULE_FROZEN_MPY       (1)
#define MICROPY_PY_ASYNC_AWAIT          (0)
#define MICROPY_PY_BUILTINS_EVAL_EXEC   (0)
#define MICROPY_PY___FILE__             (0)
#define MICROPY_PY_IO_BYTESIO           (0)
#define MICROPY_PY_SYS                  (0)
#define MICROPY_PY_SYS_MODULES          (0)
#define MICROPY_PY_SYS_EXIT             (0)
#define MICROPY_PY_UERRNO_ERRORCODE     (0)
#define MICROPY_PY_ARRAY                (0)
#define MICROPY_PY_BUILTINS_ENUMERATE   (0)
#define MICROPY_PY_COLLECTIONS          (0)
#define MICROPY_ENABLE_DOC_STRING       (0)
#define MICROPY_PY_BUILTINS_PROPERTY    (1)
#define MICROPY_PY_BUILTINS_REVERSED    (0)
#define MICROPY_PY_BUILTINS_SLICE       (0)
#define MICROPY_LONGINT_IMPL            (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_FLOAT_IMPL              (MICROPY_FLOAT_IMPL_DOUBLE)
#define MICROPY_PY_MATH                 (0)
#define MICROPY_PY_CMATH                (0)
#define MICROPY_USE_INTERNAL_PRINTF     (0)
#define MICROPY_PERSISTENT_CODE_LOAD    (1)
#define MICROPY_PERSISTENT_CODE_SAVE    (0)
#define MICROPY_PY_BUILTINS_STR_UNICODE (1)
#define MICROPY_PY_STRUCT               (1)
#define MICROPY_PY_UBINASCII            (1)
#define MICROPY_PY_USSL                 (1)
#define MICROPY_SSL_MBEDTLS             (1)
#define MICROPY_PY_IO                   (1)
#define MICROPY_PY_IO_IOBASE            (1)
#define MICROPY_PY_IO_FILEIO            (1)

extern const struct _mp_obj_module_t cc_mp_module_port;
extern const struct _mp_obj_module_t cc_mp_module_calvinsys;
extern const struct _mp_obj_module_t cc_mp_module_socket;

#define MICROPY_PY_CC_PORT { MP_ROM_QSTR(MP_QSTR_cc_mp_port), MP_ROM_PTR(&cc_mp_module_port) },
#define MICROPY_PY_CC_CALVINSYS { MP_ROM_QSTR(MP_QSTR_cc_mp_calvinsys), MP_ROM_PTR(&cc_mp_module_calvinsys) },
#define MICROPY_PY_CC_SOCKET { MP_ROM_QSTR(MP_QSTR_usocket), MP_ROM_PTR(&cc_mp_module_socket) },

#define MICROPY_PORT_BUILTIN_MODULES \
  MICROPY_PY_CC_PORT \
  MICROPY_PY_CC_CALVINSYS \
  MICROPY_PY_CC_SOCKET \

#define MICROPY_PORT_BUILTIN_MODULE_WEAK_LINKS \
  { MP_ROM_QSTR(MP_QSTR_binascii), MP_ROM_PTR(&mp_module_ubinascii) }, \
  { MP_ROM_QSTR(MP_QSTR_struct), MP_ROM_PTR(&mp_module_ustruct) }, \

#define MICROPY_PORT_BUILTINS \
  { MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&mp_builtin_open_obj) },

#define MP_PLAT_PRINT_STRN(str, len) cc_log(str)

//////////////////////////////////////////
// Do not change anything beyond this line
//////////////////////////////////////////

// Define to 1 to use undertested inefficient GC helper implementation
// (if more efficient arch-specific one is not available).
#ifndef MICROPY_GCREGS_SETJMP
  #ifdef __mips__
    #define MICROPY_GCREGS_SETJMP (1)
  #else
    #define MICROPY_GCREGS_SETJMP (0)
  #endif
#endif

// type definitions for the specific machine

#ifdef __LP64__
typedef long mp_int_t; // must be pointer size
typedef unsigned long mp_uint_t; // must be pointer size
#else
// These are definitions for machines where sizeof(int) == sizeof(void*),
// regardless for actual size.
typedef int mp_int_t; // must be pointer size
typedef unsigned int mp_uint_t; // must be pointer size
#endif

// Cannot include <sys/types.h>, as it may lead to symbol name clashes
#if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64 && !defined(__LP64__))
typedef long long mp_off_t;
#else
typedef long mp_off_t;
#endif

// We need to provide a declaration/definition of alloca()
#ifdef __FreeBSD__
#include <stdlib.h>
#else
#include <alloca.h>
#endif

#endif
