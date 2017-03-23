PLATFORM := $(shell uname)
CC = gcc
PROJECT_NAME = calvin_c

CFLAGS  = -g -Wall
CFLAGS += --std=gnu99
#CFLAGS += -DDEBUG
CFLAGS += -DPARSE_ARGS
ifneq ($(PLATFORM), Darwin)
CFLAGS += -lrt
endif
CFLAGS += -DUSE_PERSISTENT_STORAGE -DTRANSPORT_SOCKET

C_SOURCE_FILES += \
platform/x86/platform_x86.c \
api.c \
main.c \
common.c \
node.c \
proto.c \
transport.c \
transport/socket/transport_socket.c \
tunnel.c \
link.c \
actor.c \
port.c \
fifo.c \
token.c \
msgpack_helper.c \
msgpuck/msgpuck.c \
actors/actor_identity.c \
actors/actor_gpioreader.c \
actors/actor_gpiowriter.c \
actors/actor_temperature.c

c_calvin: $(C_SOURCE_FILES)
	@sed -i -e 's/mp_decode_uint/mpk_decode_uint/' msgpuck/msgpuck.h
	$(CC) -o $(PROJECT_NAME) $(C_SOURCE_FILES) $(CFLAGS)

clean:
	rm -f $(PROJECT_NAME)
