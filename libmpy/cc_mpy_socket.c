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
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include "cc_mpy_socket.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"

// TODO: Use constrained platform networking functions

typedef struct _mp_obj_socket_t {
  mp_obj_base_t base;
  int fd;
  bool blocking;
} mp_obj_socket_t;

STATIC const mp_obj_type_t mp_type_socket;
static cc_calvinsys_t *calvinsys;

void cc_mpy_socket_set_calvinsys(cc_calvinsys_t *sys)
{
  calvinsys = sys;
}

STATIC mp_obj_socket_t *cc_mpy_socket_new(int fd)
{
  mp_obj_socket_t *o = m_new_obj(mp_obj_socket_t);

  o->base.type = &mp_type_socket;
  o->fd = fd;
  o->blocking = false;

  return o;
}

STATIC mp_obj_t cc_mpy_socket_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
  (void)type_in;
  (void)n_kw;

  int family = AF_INET;
  int type = SOCK_STREAM;
  int proto = 0;
  int i = 0, fd_pos = -1;

	for (i = 0; i < CC_CALVINSYS_MAX_FDS; i++) {
		if (calvinsys->fds[i] == -1) {
			fd_pos = i;
			break;
		}
	}

	if (fd_pos == -1) {
		cc_log_error("No free descriptors");
		return mp_const_none;
	}

  if (n_args > 0) {
    if (!MP_OBJ_IS_SMALL_INT(args[0])) {
      cc_log_error("Bad type");
      return mp_const_none;
    }

    family = MP_OBJ_SMALL_INT_VALUE(args[0]);
    if (n_args > 1) {
      if (!MP_OBJ_IS_SMALL_INT(args[1])) {
        cc_log_error("Bad type");
        return mp_const_none;
      }

      type = MP_OBJ_SMALL_INT_VALUE(args[1]);
      if (n_args > 2) {
        if (!MP_OBJ_IS_SMALL_INT(args[2])) {
          cc_log_error("Bad type");
          return mp_const_none;
        }

        proto = MP_OBJ_SMALL_INT_VALUE(args[2]);
      }
    }
  }

  int fd = socket(family, type, proto);
  if (fd < 0) {
    cc_log_error("Failed to create socket");
    return mp_const_none;
  }

  calvinsys->fds[fd_pos] = fd;

  return MP_OBJ_FROM_PTR(cc_mpy_socket_new(fd));
}

STATIC mp_obj_t cc_mpy_socket_connect(mp_obj_t self_in, mp_obj_t addr_in)
{
  mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
  mp_buffer_info_t bufinfo;
  mp_get_buffer_raise(addr_in, &bufinfo, MP_BUFFER_READ);
  int r = connect(self->fd, (const struct sockaddr *)bufinfo.buf, bufinfo.len);

  if (r != 0) {
    cc_log_error("Failed to connect socket");
  }

  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(cc_mpy_socket_connect_obj, cc_mpy_socket_connect);

STATIC mp_obj_t cc_mpy_socket_send(size_t n_args, const mp_obj_t *args)
{
  mp_obj_socket_t *self = MP_OBJ_TO_PTR(args[0]);
  int flags = 0;

  if (n_args > 2) {
    flags = MP_OBJ_SMALL_INT_VALUE(args[2]);
  }

  mp_buffer_info_t bufinfo;
  mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);
  int out_sz = send(self->fd, bufinfo.buf, bufinfo.len, flags);

  if (out_sz == -1) {
    cc_log_error("Failed to send data");
    return mp_const_none;
  }

  return MP_OBJ_NEW_SMALL_INT(out_sz);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(cc_mpy_socket_send_obj, 2, 3, cc_mpy_socket_send);

STATIC mp_obj_t cc_mpy_socket_recv(size_t n_args, const mp_obj_t *args)
{
  mp_obj_socket_t *self = MP_OBJ_TO_PTR(args[0]);
  int sz = MP_OBJ_SMALL_INT_VALUE(args[1]);
  int flags = 0;

  if (n_args > 2) {
      flags = MP_OBJ_SMALL_INT_VALUE(args[2]);
  }

  byte *buf = m_new(byte, sz);
  int out_sz = recv(self->fd, buf, sz, flags);
  if (out_sz == -1) {
    cc_log_error("Failed to read data");
    return mp_const_none;
  }

  mp_obj_t ret = mp_obj_new_str_of_type(&mp_type_bytes, buf, out_sz);
  m_del(char, buf, sz);
  return ret;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(cc_mpy_socket_recv_obj, 2, 3, cc_mpy_socket_recv);

STATIC mp_obj_t socket_setblocking(mp_obj_t self_in, mp_obj_t flag_in)
{
  mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
  int val = mp_obj_is_true(flag_in);
  int flags = fcntl(self->fd, F_GETFL, 0);

  if (val) {
    flags &= ~O_NONBLOCK;
  } else {
    flags |= O_NONBLOCK;
  }
  flags = fcntl(self->fd, F_SETFL, flags);

  self->blocking = val;

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_setblocking_obj, socket_setblocking);

STATIC mp_obj_t cc_mpy_socket_close(mp_obj_t self_arg)
{
  mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_arg);
  int i = 0;

  mp_stream_close(self_arg);

  for (i = 0; i < CC_CALVINSYS_MAX_FDS; i++) {
    if (calvinsys->fds[i] == self->fd) {
      calvinsys->fds[i] = -1;
      break;
    }
  }

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(cc_mpy_socket_close_obj, cc_mpy_socket_close);

STATIC const mp_rom_map_elem_t socket_locals_dict_table[] = {
  { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&cc_mpy_socket_connect_obj) },
  { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&cc_mpy_socket_send_obj) },
  { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
  { MP_ROM_QSTR(MP_QSTR_recv), MP_ROM_PTR(&cc_mpy_socket_recv_obj) },
  { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
  { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
  { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
  { MP_ROM_QSTR(MP_QSTR_setblocking), MP_ROM_PTR(&socket_setblocking_obj) },
  { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&cc_mpy_socket_close_obj) },
};

STATIC MP_DEFINE_CONST_DICT(socket_locals_dict, socket_locals_dict_table);

STATIC mp_uint_t socket_read(mp_obj_t o_in, void *buf, mp_uint_t size, int *errcode)
{
  mp_obj_socket_t *o = MP_OBJ_TO_PTR(o_in);
  mp_int_t r = read(o->fd, buf, size);
  if (r == -1) {
    int err = errno;
    // On blocking socket, we get EAGAIN in case SO_RCVTIMEO/SO_SNDTIMEO
    // timed out, and need to convert that to ETIMEDOUT.
    if (err == EAGAIN && o->blocking) {
      err = MP_ETIMEDOUT;
    }

    *errcode = err;
    return MP_STREAM_ERROR;
  }
  return r;
}

STATIC mp_uint_t socket_write(mp_obj_t o_in, const void *buf, mp_uint_t size, int *errcode)
{
  mp_obj_socket_t *o = MP_OBJ_TO_PTR(o_in);
  mp_int_t r = write(o->fd, buf, size);
  if (r == -1) {
    int err = errno;
    // On blocking socket, we get EAGAIN in case SO_RCVTIMEO/SO_SNDTIMEO
    // timed out, and need to convert that to ETIMEDOUT.
    if (err == EAGAIN && o->blocking) {
      err = MP_ETIMEDOUT;
    }

    *errcode = err;
    return MP_STREAM_ERROR;
  }
  return r;
}

STATIC mp_uint_t socket_ioctl(mp_obj_t o_in, mp_uint_t request, uintptr_t arg, int *errcode)
{
  mp_obj_socket_t *self = MP_OBJ_TO_PTR(o_in);
  (void)arg;
  switch (request) {
    case MP_STREAM_CLOSE:
      // There's a POSIX drama regarding return value of close in general,
      // and EINTR error in particular. See e.g.
      // http://lwn.net/Articles/576478/
      // http://austingroupbugs.net/view.php?id=529
      // The rationale MicroPython follows is that close() just releases
      // file descriptor. If you're interested to catch I/O errors before
      // closing fd, fsync() it.
      close(self->fd);
      return 0;

    case MP_STREAM_GET_FILENO:
      return self->fd;

    default:
      *errcode = MP_EINVAL;
      return MP_STREAM_ERROR;
  }
}

STATIC const mp_stream_p_t usocket_stream_p = {
    .read = socket_read,
    .write = socket_write,
    .ioctl = socket_ioctl,
};

STATIC const mp_obj_type_t mp_type_socket = {
  { &mp_type_type },
/*
  .name = MP_QSTR_socket,
  .make_new = cc_mpy_socket_make_new,
  .locals_dict = (mp_obj_t)&socket_locals_dict,
*/
  .name = MP_QSTR_socket,
  .print = NULL,//socket_print,
  .make_new = cc_mpy_socket_make_new,
  .getiter = NULL,
  .iternext = NULL,
  .protocol = &usocket_stream_p,
  .locals_dict = (mp_obj_dict_t*)&socket_locals_dict,
};

STATIC mp_obj_t mod_socket_getaddrinfo(size_t n_args, const mp_obj_t *args)
{
  // TODO: Implement 5th and 6th args

  const char *host = mp_obj_str_get_str(args[0]);
  const char *serv = NULL;
  struct addrinfo hints;
  char buf[6];
  memset(&hints, 0, sizeof(hints));
  // getaddrinfo accepts port in string notation, so however
  // it may seem stupid, we need to convert int to str
  if (MP_OBJ_IS_SMALL_INT(args[1])) {
    unsigned port = (unsigned short)MP_OBJ_SMALL_INT_VALUE(args[1]);
    snprintf(buf, sizeof(buf), "%u", port);
    serv = buf;
    hints.ai_flags = AI_NUMERICSERV;
#ifdef __UCLIBC_MAJOR__
#if __UCLIBC_MAJOR__ == 0 && (__UCLIBC_MINOR__ < 9 || (__UCLIBC_MINOR__ == 9 && __UCLIBC_SUBLEVEL__ <= 32))
  // "warning" requires -Wno-cpp which is a relatively new gcc option, so we choose not to use it.
  //#warning Working around uClibc bug with numeric service name
  // Older versions og uClibc have bugs when numeric ports in service
  // arg require also hints.ai_socktype (or hints.ai_protocol) != 0
  // This actually was fixed in 0.9.32.1, but uClibc doesn't allow to
  // test for that.
  // http://git.uclibc.org/uClibc/commit/libc/inet/getaddrinfo.c?id=bc3be18145e4d5
  // Note that this is crude workaround, precluding UDP socket addresses
  // to be returned. TODO: set only if not set by Python args.
  hints.ai_socktype = SOCK_STREAM;
#endif
#endif
  } else {
    serv = mp_obj_str_get_str(args[1]);
  }

  if (n_args > 2) {
    hints.ai_family = MP_OBJ_SMALL_INT_VALUE(args[2]);
    if (n_args > 3) {
      hints.ai_socktype = MP_OBJ_SMALL_INT_VALUE(args[3]);
    }
  }

  struct addrinfo *addr_list;
  int res = getaddrinfo(host, serv, &hints, &addr_list);

  if (res != 0) {
    // CPython: socket.gaierror
    nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "[addrinfo error %d]", res));
  }
  assert(addr_list);

  mp_obj_t list = mp_obj_new_list(0, NULL);
  for (struct addrinfo *addr = addr_list; addr; addr = addr->ai_next) {
    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(5, NULL));
    t->items[0] = MP_OBJ_NEW_SMALL_INT(addr->ai_family);
    t->items[1] = MP_OBJ_NEW_SMALL_INT(addr->ai_socktype);
    t->items[2] = MP_OBJ_NEW_SMALL_INT(addr->ai_protocol);
    // "canonname will be a string representing the canonical name of the host
    // if AI_CANONNAME is part of the flags argument; else canonname will be empty." ??
    if (addr->ai_canonname) {
      t->items[3] = MP_OBJ_NEW_QSTR(qstr_from_str(addr->ai_canonname));
    } else {
      t->items[3] = mp_const_none;
    }
    t->items[4] = mp_obj_new_bytearray(addr->ai_addrlen, addr->ai_addr);
    mp_obj_list_append(list, MP_OBJ_FROM_PTR(t));
  }
  freeaddrinfo(addr_list);
  return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_socket_getaddrinfo_obj, 2, 4, mod_socket_getaddrinfo);

STATIC const mp_rom_map_elem_t mp_module_usocket_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__),  MP_ROM_QSTR(MP_QSTR_usocket) },
  { MP_ROM_QSTR(MP_QSTR_socket), MP_ROM_PTR(&mp_type_socket) },
  { MP_ROM_QSTR(MP_QSTR_getaddrinfo), MP_ROM_PTR(&mod_socket_getaddrinfo_obj) },
#define C(name) { MP_ROM_QSTR(MP_QSTR_ ## name), MP_ROM_INT(name) }
  C(AF_UNIX),
  C(AF_INET),
  C(AF_INET6),
  C(SOCK_STREAM),
  C(SOCK_DGRAM),
  C(SOCK_RAW),

  C(MSG_DONTROUTE),
  C(MSG_DONTWAIT),

  C(SOL_SOCKET),
  C(SO_BROADCAST),
  C(SO_ERROR),
  C(SO_KEEPALIVE),
  C(SO_LINGER),
  C(SO_REUSEADDR),
#undef C
};

STATIC MP_DEFINE_CONST_DICT(mp_module_usocket_globals, mp_module_usocket_globals_table);

const mp_obj_module_t cc_mp_module_socket = {
  .base = { &mp_type_module },
  .globals = (mp_obj_dict_t*)&mp_module_usocket_globals,
};
