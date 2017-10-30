PLATFORM := $(shell uname)
CC = gcc
PROJECT_NAME = calvin_c
BUILD = build

CFLAGS  = -Wall
CFLAGS += -Os -fdata-sections -ffunction-sections
#CFLAGS += -g -fsanitize=address -O1 -fno-omit-frame-pointer
CFLAGS += -I.
LDFLAGS += -Wl,--gc-sections

# platform sources
SRC_C = main.c runtime/south/platform/x86/cc_platform_x86.c

# calvin sources and parameters
include calvin.mk

calvin: rename_symbol $(SRC_C)
	@echo "Building calvin without MicroPython"
	$(CC) $(CFLAGS) $(SRC_C) -o $(PROJECT_NAME) $(LDFLAGS)

mpy: rename_symbol mpy-cross -lmicropython $(SRC_C) $(SRC_C_MPY)
	@echo "Building calvin with MicroPython"
	$(CC) $(SRC_C) $(SRC_C_MPY) -o $(PROJECT_NAME) $(CFLAGS) $(CFLAGSMPY) $(LDFLAGS)

rename_symbol:
	@echo "Renaming mp_decode_uint in msgpuck/msgpuck.h"
	@sed -i -e 's/mp_decode_uint/mpk_decode_uint/' msgpuck/msgpuck.h

mpy-cross:
	@echo "Building the MicroPython cross compiler"
	$(MAKE) -C micropython/mpy-cross

-lmicropython:
	@echo "Building the MicroPython lib"
	$(MAKE) lib -C libmpy

clean:
	rm -f $(PROJECT_NAME)
	$(MAKE) clean -C libmpy
