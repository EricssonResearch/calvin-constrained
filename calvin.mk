# runtime sources
CC_SRC_C += \
	cc_api.c \
	runtime/north/cc_common.c \
	runtime/north/scheduler/np_scheduler/cc_scheduler.c \
	runtime/north/cc_node.c \
	runtime/north/cc_proto.c \
	runtime/north/cc_transport.c \
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

# features
ifeq ($(CC_GETOPT_ENABLED),1)
CC_CFLAGS += -DCC_GETOPT_ENABLED
endif

ifeq ($(CC_DEEPSLEEP_ENABLED),1)
ifndef CC_SLEEP_TIME
$(info CC_SLEEP_TIME not set, using default 60 seconds)
CC_SLEEP_TIME=60
endif
# storage is required with sleep
CC_CFLAGS += -DCC_STORAGE_ENABLED
CC_CFLAGS += -DCC_DEEPSLEEP_ENABLED -DCC_SLEEP_TIME=$(CC_SLEEP_TIME)
endif

ifeq ($(CC_STORAGE_ENABLED),1)
CC_CFLAGS += -DCC_STORAGE_ENABLED
endif

# transports

ifeq ($(CC_TRANSPORT_SOCKET),1)
CC_SRC_C += runtime/south/transport/socket/cc_transport_socket.c
CC_CFLAGS += -DCC_TRANSPORT_SOCKET
ifeq ($(CC_LWIP_SOCKET),1)
CC_CFLAGS += -DCC_LWIP_SOCKET
endif
endif

ifeq ($(CC_TRANSPORT_LWIP),1)
CC_SRC_C += runtime/south/transport/lwip/cc_transport_lwip.c
CC_CFLAGS += -DCC_TRANSPORT_LWIP
endif

ifeq ($(CC_TRANSPORT_FCM),1)
CC_SRC_C += runtime/south/transport/fcm/cc_transport_fcm.c
CC_CFLAGS += -DCC_TRANSPORT_FCM
endif

ifeq ($(CC_TRANSPORT_SPRITZER),1)
CC_SRC_C += runtime/south/transport/spritzer/cc_transport_spritzer.c
CC_CFLAGS += -DCC_TRANSPORT_SPRITZER
endif

# C actors

ifeq ($(CC_ACTOR_IDENTITY),1)
CC_SRC_C += actors/cc_actor_identity.c
CC_CFLAGS += -DCC_ACTOR_IDENTITY
endif

ifeq ($(CC_ACTOR_LIGHT),1)
CC_SRC_C += actors/cc_actor_light.c
CC_CFLAGS += -DCC_ACTOR_LIGHT
endif

ifeq ($(CC_ACTOR_BUTTON),1)
CC_SRC_C += actors/cc_actor_button.c
CC_CFLAGS += -DCC_ACTOR_BUTTON
endif

ifeq ($(CC_ACTOR_TEMPERATURE),1)
CC_SRC_C += actors/cc_actor_temperature.c
CC_CFLAGS += -DCC_ACTOR_TEMPERATURE
endif

ifeq ($(CC_ACTOR_REGISTRY_ATTIBUTE),1)
CC_SRC_C += actors/cc_actor_registry_attribute.c
CC_CFLAGS += -DCC_ACTOR_REGISTRY_ATTIBUTE
endif

ifeq ($(CC_ACTOR_TEMPERATURE_TAGGED),1)
CC_SRC_C += actors/cc_actor_temperature_tagged.c
CC_CFLAGS += -DCC_ACTOR_TEMPERATURE_TAGGED
endif

# MicroPython config
CC_CFLAGSMPY = -std=gnu99
CC_CFLAGSMPY += -Llibmpy -lmicropython -lm -Ilibmpy/build -Imicropython -Ilibmpy
CC_CFLAGSMPY += -DCC_PYTHON_ENABLED -DCC_PYTHON_HEAP_SIZE=20*1024 -DCC_PYTHON_STACK_SIZE=8192
CC_CFLAGSMPY += -DCC_ACTOR_MODULES_DIR=\""mpys/\""
CC_SRC_C_MPY += actors/cc_actor_mpy.c libmpy/cc_mpy_port.c libmpy/cc_mpy_calvinsys.c
