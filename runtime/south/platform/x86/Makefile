PLATFORM := $(shell uname)
CC = gcc
PROJECT_NAME = calvin_c
BUILD = build

CFLAGS  = -Wall
CFLAGS += -Os -fdata-sections -ffunction-sections
#CFLAGS += -g -fsanitize=address -O1 -fno-omit-frame-pointer
CFLAGS += -DCC_GETOPT_ENABLED
CFLAGS += -DCC_TRANSPORT_SOCKET -DCC_TRANSPORT_SOCKET_SSDP_ENABLED
CFLAGS += -DCC_STORAGE_ENABLED -DCC_DEEPSLEEP_ENABLED -DCC_SLEEP_TIME=60 -DCC_RUNTIME_STATE_BUFFER_SIZE=10000
LDFLAGS += -Wl,--gc-sections

# runtime sources
SRC_C = \
	runtime/south/platform/x86/cc_platform_x86.c \
	cc_api.c \
	main.c \
	runtime/north/cc_common.c \
	runtime/north/scheduler/np_scheduler/cc_scheduler.c \
	runtime/north/cc_node.c \
	runtime/north/cc_proto.c \
	runtime/north/cc_transport.c \
	runtime/south/transport/socket/cc_transport_socket.c \
	runtime/north/cc_tunnel.c \
	runtime/north/cc_link.c \
	runtime/north/cc_actor_store.c \
	runtime/north/cc_actor.c \
	runtime/north/cc_port.c \
	runtime/north/cc_fifo.c \
	runtime/north/cc_token.c \
	msgpuck/msgpuck.c \
	runtime/north/coder/cc_coder_msgpuck.c \
	calvinsys/cc_calvinsys.c \
	calvinsys/common/cc_calvinsys_timer.c \
	calvinsys/common/cc_calvinsys_attribute.c

# C actors
CC_ACTOR_IDENTITY ?= 0
CC_ACTOR_LIGHT ?= 0
CC_ACTOR_BUTTON ?= 0
CC_ACTOR_TEMPERATURE ?= 0
CC_ACTOR_REGISTRY_ATTIBUTE ?= 0
CC_ACTOR_TEMPERATURE_TAGGED ?= 0

ifeq ($(CC_ACTOR_IDENTITY),1)
SRC_C += actors/cc_actor_identity.c
CFLAGS += -DCC_ACTOR_IDENTITY
endif

ifeq ($(CC_ACTOR_LIGHT),1)
SRC_C += actors/cc_actor_light.c
CFLAGS += -DCC_ACTOR_LIGHT
endif

ifeq ($(CC_ACTOR_BUTTON),1)
SRC_C += actors/cc_actor_button.c
CFLAGS += -DCC_ACTOR_BUTTON
endif

ifeq ($(CC_ACTOR_TEMPERATURE),1)
SRC_C += actors/cc_actor_temperature.c
CFLAGS += -DCC_ACTOR_TEMPERATURE
endif

ifeq ($(CC_ACTOR_REGISTRY_ATTIBUTE),1)
SRC_C += actors/cc_actor_registry_attribute.c
CFLAGS += -DCC_ACTOR_REGISTRY_ATTIBUTE
endif

ifeq ($(CC_ACTOR_TEMPERATURE_TAGGED),1)
SRC_C += actors/cc_actor_temperature_tagged.c
CFLAGS += -DCC_ACTOR_TEMPERATURE_TAGGED
endif

# Python actors
CFLAGSMPY = -std=gnu99
CFLAGSMPY += -Llibmpy -lmicropython -lm -Ilibmpy/build -Imicropython -Ilibmpy -DCC_PYTHON_ENABLED -DCC_PYTHON_HEAP_SIZE=20*1024 -DCC_PYTHON_STACK_SIZE=8192
CFLAGSMPY += -DCC_ACTOR_MODULES_DIR=\""mpys/\""
MP_LIB_DIR = libmpy
SRC_C_MPY += actors/cc_actor_mpy.c libmpy/cc_mpy_port.c libmpy/cc_mpy_calvinsys.c

calvin: rename_symbol $(SRC_C)
	$(CC) $(CFLAGS) $(SRC_C) -o $(PROJECT_NAME) $(LDFLAGS)

mpy-cross:
	@echo "Building the MicroPython cross compiler"
	$(MAKE) -C micropython/mpy-cross

-lmicropython:
	@echo "Building the MicroPython lib"
	$(MAKE) lib -C $(MP_LIB_DIR)

rename_symbol:
	@echo "Renaming mp_decode_uint in msgpuck/msgpuck.h"
	@sed -i -e 's/mp_decode_uint/mpk_decode_uint/' msgpuck/msgpuck.h

mpy: rename_symbol mpy-cross -lmicropython $(SRC_C) $(SRC_C_MPY)
	$(CC) $(SRC_C) $(SRC_C_MPY) -o $(PROJECT_NAME) $(CFLAGS) $(CFLAGSMPY) $(LDFLAGS)

clean:
	rm -f $(PROJECT_NAME)
	$(MAKE) clean -C $(MP_LIB_DIR)
