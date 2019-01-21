PLATFORM := $(shell uname)
CC = gcc
PROJECT_NAME = calvin_c
BUILD = build

CC_CFLAGS  = -Wall
#CC_CFLAGS += -Os -fdata-sections -ffunction-sections
CC_CFLAGS += -ggdb -Og -g3 -fno-omit-frame-pointer
CC_CFLAGS += -I.
CC_CFLAGS += -DCC_USE_FDS=1
ifeq ($(PLATFORM), Darwin)
CC_LDFLAGS += -Wl,-dead_strip
else
CC_LDFLAGS += -Wl,--gc-sections
endif

# platform sources
CC_SRC_C = main.c runtime/south/platform/x86/cc_platform_x86.c runtime/south/transport/socket/cc_transport_socket.c

# Calvinsys sources
#CC_SRC_C += $(wildcard runtime/south/platform/x86/calvinsys/*.c)
CC_SRC_C += runtime/south/platform/x86/calvinsys/cc_test_gpio.c
CC_SRC_C += runtime/south/platform/x86/calvinsys/cc_test_temperature.c

ifeq ($(LIBCOAP),1)
CC_SRC_C += runtime/south/platform/x86/calvinsys/cc_libcoap_client.c
#CC_CFLAGS += -DCC_USE_LIBCOAP_CLIENT=1
#CC_CFLAGS += -I/usr/local/include/coap2
#CC_LDFLAGS += -L/usr/local/lib -lcoap-2
CC_CFLAGS += $(shell pkg-config --cflags-only-I libcoap-1)
CC_LDFLAGS += $(shell pkg-config --libs-only-L libcoap-1)
CC_LIBS += $(shell pkg-config --libs-only-l libcoap-1)
endif

# calvin sources and parameters
include calvin.mk

calvin: rename_symbol $(CC_SRC_C)
	@echo "Building calvin"
	$(CC) $(CC_SRC_C) -o $(PROJECT_NAME) $(CC_CFLAGS) $(CC_LDFLAGS) $(CC_LIBS)

rename_symbol:
	@echo "Renaming mp_decode_uint in msgpuck/msgpuck.h"
	@sed -i -e 's/mp_decode_uint/mpk_decode_uint/' msgpuck/msgpuck.h

clean:
	rm -f $(PROJECT_NAME)
