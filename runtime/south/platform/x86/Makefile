PLATFORM := $(shell uname)
CC = gcc
PROJECT_NAME = calvin_c

CFLAGS  = -g -Wall
CFLAGS += --std=gnu99
ifneq ($(PLATFORM), Darwin)
CFLAGS += -lrt
endif
CFLAGS += -DCC_GETOPT_ENABLED
CFLAGS += -DCC_TRANSPORT_SOCKET -DCC_TRANSPORT_SOCKET_SSDP_ENABLED
CFLAGS += -DCC_STORAGE_ENABLED -DCC_DEEPSLEEP_ENABLED -DCC_INACTIVITY_TIMEOUT=5 -DCC_SLEEP_TIME=60 -DCC_RUNTIME_STATE_BUFFER_SIZE=10000
CFLAGS += -DCC_ACTOR_IDENTIY -DCC_ACTOR_BUTTON -DCC_ACTOR_LIGHT -DCC_ACTOR_SOIL_MOISTURE -DCC_ACTOR_TEMPERATURE

C_SOURCE_FILES += \
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
runtime/north/cc_msgpack_helper.c \
msgpuck/msgpuck.c \
calvinsys/cc_calvinsys.c \
actors/cc_actor_identity.c \
actors/cc_actor_light.c \
actors/cc_actor_button.c \
actors/cc_actor_temperature.c \
actors/cc_actor_soil_moisture.c

c_calvin: $(C_SOURCE_FILES)
	@sed -i -e 's/mp_decode_uint/mpk_decode_uint/' msgpuck/msgpuck.h
	$(CC) -o $(PROJECT_NAME) $(C_SOURCE_FILES) $(CFLAGS) $(INC_PATHS)

clean:
	rm -f $(PROJECT_NAME)
