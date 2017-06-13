/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef CCMP_CONFIGFILE
#include CCMP_CONFIGFILE
#endif

#include <stdint.h>

// options to control how Micro Python is built

#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_QSTR_BYTES_IN_HASH  (1)
#define MICROPY_ALLOC_PATH_MAX      (128)
#define MICROPY_ENABLE_GC           (1)
#define MICROPY_EMIT_ARM		    (0)
#define MICROPY_COMP_MODULE_CONST   (0)
#define MICROPY_COMP_CONST          (0)
#define MICROPY_COMP_DOUBLE_TUPLE_ASSIGN (0)
#define MICROPY_COMP_TRIPLE_TUPLE_ASSIGN (0)
#define MICROPY_MEM_STATS           (0)
#define MICROPY_DEBUG_PRINTERS      (0)
#define MICROPY_HELPER_REPL         (0)
#define MICROPY_HELPER_LEXER_UNIX   (0)
#define MICROPY_ENABLE_SOURCE_LINE  (0)
#define MICROPY_ENABLE_DOC_STRING   (0)
#define MICROPY_ERROR_REPORTING     (MICROPY_ERROR_REPORTING_TERSE)
#define MICROPY_BUILTIN_METHOD_CHECK_SELF_ARG (0)
#define MICROPY_PY_ASYNC_AWAIT (0)
#define MICROPY_PY_BUILTINS_FLOAT   (1)
#define MICROPY_PY_BUILTINS_BYTEARRAY (1)
#define MICROPY_PY_BUILTINS_MEMORYVIEW (0)
#define MICROPY_PY_BUILTINS_ENUMERATE (0)
#define MICROPY_PY_BUILTINS_FROZENSET (0)
#define MICROPY_PY_BUILTINS_REVERSED (0)
#define MICROPY_PY_BUILTINS_SET     (1)
#define MICROPY_PY_BUILTINS_SLICE   (0)
#define MICROPY_PY_BUILTINS_PROPERTY (0)
#define MICROPY_PY___FILE__         (0)
#define MICROPY_PY_GC               (1)
#define MICROPY_PY_ARRAY            (0)
#define MICROPY_PY_ATTRTUPLE        (0)
#define MICROPY_PY_COLLECTIONS      (0)
#define MICROPY_PY_MATH             (0)
#define MICROPY_PY_CMATH            (0)
#define MICROPY_PY_IO               (0)
#define MICROPY_PY_STRUCT           (0)
#define MICROPY_PY_SYS              (0)
#define MICROPY_CPYTHON_COMPAT      (1)
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_DOUBLE)//(MICROPY_FLOAT_IMPL_NONE)
#define MICROPY_USE_INTERNAL_PRINTF (0)
#define MICROPY_MODULE_FROZEN_MPY   (1)

// type definitions for the specific machine

#define BYTES_PER_WORD sizeof(mp_int_t)

#define MICROPY_MAKE_POINTER_CALLABLE(p) ((void *)((mp_uint_t)(p) | 1))

#define UINT_FMT "%lu"
#define INT_FMT "%ld"

extern const struct _mp_obj_module_t cc_mp_module_port;
extern const struct _mp_obj_module_t cc_mp_module_calvinsys;
#define MICROPY_PORT_BUILTIN_MODULES \
	{ MP_OBJ_NEW_QSTR(MP_QSTR_cc_mp_port), (mp_obj_t)&cc_mp_module_port }, \
	{ MP_OBJ_NEW_QSTR(MP_QSTR_cc_mp_calvinsys), (mp_obj_t)&cc_mp_module_calvinsys }, \

// assume that if we already defined the obj repr then we also defined types
#ifndef MICROPY_OBJ_REPR
#ifdef __LP64__
typedef long mp_int_t; // must be pointer size
typedef unsigned long mp_uint_t; // must be pointer size
#else
// These are definitions for machines where sizeof(int) == sizeof(void*),
// regardless of actual size.
typedef int mp_int_t; // must be pointer size
typedef unsigned int mp_uint_t; // must be pointer size
#endif
#endif

typedef long mp_off_t;

#define MICROPY_PERSISTENT_CODE_LOAD (0)
#define MICROPY_ENABLE_COMPILER     (0)
#define MICROPY_ENABLE_FINALISER    (0)
#define MICROPY_STACK_CHECK         (0)
#define MICROPY_MALLOC_USES_ALLOCATED_SIZE (0)
// Printing debug to stderr may give tests which
// check stdout a chance to pass, etc.
#define MICROPY_DEBUG_PRINTER_DEST  mp_stderr_print
#define MICROPY_USE_READLINE_HISTORY (0)
#define MICROPY_REPL_EMACS_KEYS     (0)
#define MICROPY_REPL_AUTO_INDENT    (0)
#define MICROPY_STREAMS_NON_BLOCK   (0)
#define MICROPY_STREAMS_POSIX_API   (0)
#define MICROPY_OPT_COMPUTED_GOTO   (0)
#ifndef MICROPY_OPT_CACHE_MAP_LOOKUP_IN_BYTECODE
#define MICROPY_OPT_CACHE_MAP_LOOKUP_IN_BYTECODE (1)
#endif
#define MICROPY_CAN_OVERRIDE_BUILTINS (0)
#define MICROPY_PY_FUNCTION_ATTRS   (0)
#define MICROPY_PY_DESCRIPTORS      (0)
#define MICROPY_PY_BUILTINS_STR_UNICODE (0)
#define MICROPY_PY_BUILTINS_STR_CENTER (0)
#define MICROPY_PY_BUILTINS_STR_PARTITION (1)
#define MICROPY_PY_BUILTINS_STR_SPLITLINES (0)
#define MICROPY_PY_BUILTINS_COMPILE (0)
#define MICROPY_PY_BUILTINS_NOTIMPLEMENTED (0)
#define MICROPY_PY_MICROPYTHON_MEM_INFO (1)
#define MICROPY_PY_ALL_SPECIAL_METHODS (0)
#define MICROPY_PY_ARRAY_SLICE_ASSIGN (0)
#define MICROPY_PY_BUILTINS_SLICE_ATTRS (0)
#define MICROPY_PY_SYS_EXIT         (0)
#define MICROPY_PY_SYS_MAXSIZE      (0)
#define MICROPY_PY_SYS_STDFILES     (0)
#define MICROPY_PY_SYS_EXC_INFO     (0)
#define MICROPY_PY_COLLECTIONS_ORDEREDDICT (0)
#ifndef MICROPY_PY_MATH_SPECIAL_FUNCTIONS
#define MICROPY_PY_MATH_SPECIAL_FUNCTIONS (0)
#endif
#define MICROPY_PY_IO_FILEIO        (0)
#define MICROPY_PY_GC_COLLECT_RETVAL (0)
#define MICROPY_STACKLESS           (0)
#define MICROPY_STACKLESS_STRICT    (0)

#define MICROPY_PY_OS_STATVFS       (0)
#define MICROPY_PY_UTIME            (0)
#define MICROPY_PY_UTIME_MP_HAL     (0)
#define MICROPY_PY_UERRNO           (0)
#define MICROPY_PY_UCTYPES          (0)
#define MICROPY_PY_UZLIB            (0)
#define MICROPY_PY_UJSON            (0)
#define MICROPY_PY_URE              (0)
#define MICROPY_PY_UHEAPQ           (0)
#define MICROPY_PY_UHASHLIB         (0)
#if MICROPY_PY_USSL && MICROPY_SSL_AXTLS
#define MICROPY_PY_UHASHLIB_SHA1    (0)
#endif
#define MICROPY_PY_UBINASCII        (0)
#define MICROPY_PY_UBINASCII_CRC32  (0)
#define MICROPY_PY_URANDOM          (0)
#ifndef MICROPY_PY_USELECT
#define MICROPY_PY_USELECT          (0)
#endif
#define MICROPY_PY_WEBSOCKET        (0)
#define MICROPY_PY_MACHINE          (0)
#define MICROPY_PY_MACHINE_PULSE    (0)

void mp_unix_alloc_exec(mp_uint_t min_size, void **ptr, mp_uint_t *size);
void mp_unix_free_exec(void *ptr, mp_uint_t size);
void mp_unix_mark_exec(void);
#define MP_PLAT_ALLOC_EXEC(min_size, ptr, size) mp_unix_alloc_exec(min_size, ptr, size)
#define MP_PLAT_FREE_EXEC(ptr, size) mp_unix_free_exec(ptr, size)
#ifndef MICROPY_FORCE_PLAT_ALLOC_EXEC
// Use MP_PLAT_ALLOC_EXEC for any executable memory allocation, including for FFI
// (overriding libffi own implementation)
#define MICROPY_FORCE_PLAT_ALLOC_EXEC (1)
#endif

#if MICROPY_PY_OS_DUPTERM
#define MP_PLAT_PRINT_STRN(str, len) mp_hal_stdout_tx_strn_cooked(str, len)
#else
#include <unistd.h>
#define MP_PLAT_PRINT_STRN(str, len) do { ssize_t ret = write(1, str, len); (void)ret; } while (0)
#endif

#ifdef __linux__
// Can access physical memory using /dev/mem
#define MICROPY_PLAT_DEV_MEM  (1)
#endif

// Assume that select() call, interrupted with a signal, and erroring
// with EINTR, updates remaining timeout value.
#define MICROPY_SELECT_REMAINING_TIME (1)

#ifdef __ANDROID__
#include <android/api-level.h>
#if __ANDROID_API__ < 4
// Bionic libc in Android 1.5 misses these 2 functions
#define MP_NEED_LOG2 (1)
#define nan(x) NAN
#endif
#endif

#define MICROPY_PORT_BUILTINS \
/*	{ MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&mp_builtin_open_obj) }, \
	{ MP_ROM_QSTR(MP_QSTR_input), MP_ROM_PTR(&mp_builtin_input_obj) }, \
*/
#define MP_STATE_PORT MP_STATE_VM

#define MICROPY_PORT_ROOT_POINTERS \
	mp_obj_t mpy_port_callback_obj; \


// We need to provide a declaration/definition of alloca()
#include <alloca.h>

// From "man readdir": "Under glibc, programs can check for the availability
// of the fields [in struct dirent] not defined in POSIX.1 by testing whether
// the macros [...], _DIRENT_HAVE_D_TYPE are defined."
// Other libc's don't define it, but proactively assume that dirent->d_type
// is available on a modern *nix system.
#ifndef _DIRENT_HAVE_D_TYPE
#define _DIRENT_HAVE_D_TYPE (1)
#endif
// This macro is not provided by glibc but we need it so ports that don't have
// dirent->d_ino can disable the use of this field.
#ifndef _DIRENT_HAVE_D_INO
#define _DIRENT_HAVE_D_INO (1)
#endif

#ifndef __APPLE__
// For debugging purposes, make printf() available to any source file.
#include <stdio.h>
#endif
