PLATFORM := $(shell uname)
CC = gcc
PROJECT_NAME = calvin_c
BUILD = build

CC_CFLAGS  = -Wall
CC_CFLAGS += -Os -fdata-sections -ffunction-sections
#CC_CFLAGS += -g -fsanitize=address -O1 -fno-omit-frame-pointer
CC_CFLAGS += -I.
CC_LDFLAGS += -Wl,--gc-sections

# platform sources
CC_SRC_C = main.c runtime/south/platform/x86/cc_platform_x86.c runtime/south/transport/socket/cc_transport_socket.c

# calvin sources and parameters
include calvin.mk

calvin: rename_symbol mpy-cross $(CC_SRC_C)
	@echo "Building calvin"
	$(CC) $(CC_SRC_C) -o $(PROJECT_NAME) $(CC_CFLAGS) $(CC_LIBS) $(CC_LDFLAGS)

rename_symbol:
	@echo "Renaming mp_decode_uint in msgpuck/msgpuck.h"
	@sed -i -e 's/mp_decode_uint/mpk_decode_uint/' msgpuck/msgpuck.h

mpy-cross:
	@echo "Building the MicroPython cross compiler"
	$(MAKE) -C micropython/mpy-cross

clean:
	rm -f $(PROJECT_NAME)
