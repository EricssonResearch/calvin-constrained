# Python actors

The support to execute Python actors is built on  [MicroPython](https://github.com/micropython/micropython) and by including it as a statically linked library when building the runtime.

When building, all Python modules located in the libmpy/modules folder are cross-compiled to bytecode and included in the library.

To enable support for Python actors, set the following in your Makefile and build the MicroPython cross-compiler and the MicroPython library before the runtime is built:

```
# location of the micropython library
MP_LIB_DIR = libmpy

# enable MicroPython and set its heap
CFLAGS += -DMICROPYTHON -DMICROPYTHON_HEAP_SIZE=20*1024

# set paths, include the library and depending libraries
CFLAGS += -Llibmpy -lmicropython -lm -Ilibmpy/build -Imicropython -Ilibmpy

# add sources of bindings for c to/from python
SRC_C += actors/cc_actor_mpy.c
SRC_C += $(addprefix libmpy/, cc_mpy_port.c cc_mpy_gpiohandler.c cc_mpy_environmental.c)

# build the MicroPython cross compiler, should be called before building the runtime
mpy-cross:
	$(MAKE) -C micropython/mpy-cross

# build the MicroPython lib, should be called before building the runtime
-lmicropython:
	$(MAKE) lib -C $(MP_LIB_DIR)
```

The MicroPython heap is alloacted using the platform_mem_alloc() function and the size should be configured to handle actors included in library. And the default configuration of MicroPython, libmpy/mpconfigport.h, is configured to enable a limited set of Python functionality to minimize the size. Adding new actors can imply enabling functionality.
