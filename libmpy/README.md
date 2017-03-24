# Python actors

The support to execute Python code and actors written in Python is added using [MicroPython](https://github.com/micropython/micropython). MicroPython is built and included as a statically linked library when building the runtime.

When building the library the MicroPython cross compiler is used to convert all modules located in and under libmpy/modules to bytecode which is then included in the library. The default configuration of MicroPython, libmpy/mpconfigport.h, enables a limited set of Python functionality. Adding new actors can imply enabling functionality to function.

When building, include:

```
# location of the micropython library
MP_LIB_DIR = libmpy

# enable MicroPython and set its heap
CFLAGS += -DMICROPYTHON -DMICROPYTHON_HEAP_SIZE=20*1024

# set paths, include the library and depending libraries
CFLAGS += -Llibmpy -lmicropython -lm -Ilibmpy/build -Imicropython -Ilibmpy

# add sources of bindings for c to/from python
SRC_C += actors/actor_mpy.c
SRC_C += $(addprefix libmpy/, calvin_mpy_port.c calvin_mpy_gpiohandler.c calvin_mpy_environmental.c)
```

to support Python actors.
