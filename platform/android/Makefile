PLATFORM := $(shell uname)
CC = gcc
PROJECT_NAME = calvin_c

CFLAGS  = -g -Wall
CFLAGS += --std=gnu99
#CFLAGS += -DDEBUG
CFLAGS += -DPARSE_ARGS
#CFLAGS += -DLWM2M_HTTP_CLIENT

C_SOURCE_FILES += \
platform/android/platform_android.c \
main.c \
common.c \
node.c \
proto.c \
transport_socket.c \
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
