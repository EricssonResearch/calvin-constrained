PLATFORM := $(shell uname)
CC = gcc
PROJECT_NAME = calvin_c

CC_ACTOR_IDENTITY ?= 1
CC_ACTOR_LIGHT ?= 1
CC_ACTOR_BUTTON ?= 1
CC_ACTOR_TEMPERATURE ?= 1
CC_ACTOR_REGISTRY_ATTIBUTE ?= 1
CC_ACTOR_TEMPERATURE_TAGGED ?= 1

CFLAGS  = -g -Wall
CFLAGS += --std=gnu99
ifneq ($(PLATFORM), Darwin)
CFLAGS += -lrt
endif
CFLAGS += -DCC_GETOPT_ENABLED
CFLAGS += -DCC_TRANSPORT_SOCKET -DCC_TRANSPORT_SOCKET_SSDP_ENABLED
CFLAGS += -DCC_STORAGE_ENABLED -DCC_DEEPSLEEP_ENABLED -DCC_SLEEP_TIME=60 -DCC_RUNTIME_STATE_BUFFER_SIZE=10000
CFLAGS += -DCC_ADD_STRNSTR

C_SOURCE_FILES = \
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

ifeq ($(CC_ACTOR_IDENTITY),1)
C_SOURCE_FILES += actors/cc_actor_identity.c
CFLAGS += -DCC_ACTOR_IDENTITY
endif

ifeq ($(CC_ACTOR_LIGHT),1)
C_SOURCE_FILES += actors/cc_actor_light.c
CFLAGS += -DCC_ACTOR_LIGHT
endif

ifeq ($(CC_ACTOR_BUTTON),1)
C_SOURCE_FILES += actors/cc_actor_button.c
CFLAGS += -DCC_ACTOR_BUTTON
endif

ifeq ($(CC_ACTOR_TEMPERATURE),1)
C_SOURCE_FILES += actors/cc_actor_temperature.c
CFLAGS += -DCC_ACTOR_TEMPERATURE
endif

ifeq ($(CC_ACTOR_REGISTRY_ATTIBUTE),1)
C_SOURCE_FILES += actors/cc_actor_registry_attribute.c
CFLAGS += -DCC_ACTOR_REGISTRY_ATTIBUTE
endif

ifeq ($(CC_ACTOR_TEMPERATURE_TAGGED),1)
C_SOURCE_FILES += actors/cc_actor_temperature_tagged.c
CFLAGS += -DCC_ACTOR_TEMPERATURE_TAGGED
endif

c_calvin: $(C_SOURCE_FILES)
	@sed -i -e 's/mp_decode_uint/mpk_decode_uint/' msgpuck/msgpuck.h
	$(CC) -o $(PROJECT_NAME) $(C_SOURCE_FILES) $(CFLAGS) $(INC_PATHS)

clean:
	rm -f $(PROJECT_NAME)
