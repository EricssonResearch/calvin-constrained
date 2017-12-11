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

# runtime features/config

ifndef RECONNECT_TIMEOUT
$(info RECONNECT_TIMEOUT not set, using default 5 seconds)
CC_CFLAGS += -DCC_RECONNECT_TIMEOUT=5
endif

ifeq ($(GETOPT),1)
CC_CFLAGS += -DCC_GETOPT_ENABLED
endif

ifndef INACTIVITY_TIMEOUT
$(info INACTIVITY_TIMEOUT not set, using default 2 seconds)
INACTIVITY_TIMEOUT=2
endif
CC_CFLAGS += -DCC_INACTIVITY_TIMEOUT=$(INACTIVITY_TIMEOUT)

ifeq ($(SLEEP),1)
ifndef SLEEP_TIME
$(info CC_SLEEP_TIME not set, using default 60 seconds)
SLEEP_TIME=60
endif
# State serialization required with sleep
STORAGE = 1
CC_CFLAGS += -DCC_DEEPSLEEP_ENABLED -DCC_SLEEP_TIME=$(SLEEP_TIME)
endif

ifeq ($(STORAGE),1)
ifndef CONFIG_FILE
$(info CONFIG_FILE not set, using default calvin.msgpack)
CC_CFLAGS += -DCC_CONFIG_FILE=\""calvin.msgpack\""
endif
CC_CFLAGS += -DCC_STORAGE_ENABLED
endif

# transports

ifeq ($(SOCKET),1)
CC_SRC_C += runtime/south/transport/socket/cc_transport_socket.c
CC_CFLAGS += -DCC_TRANSPORT_SOCKET
ifeq ($(CC_LWIP_SOCKET),1)
CC_CFLAGS += -DCC_LWIP_SOCKET
endif
endif

ifeq ($(LWIP),1)
CC_SRC_C += runtime/south/transport/lwip/cc_transport_lwip.c
CC_CFLAGS += -DCC_TRANSPORT_LWIP
endif

ifeq ($(FCM),1)
CC_SRC_C += runtime/south/transport/fcm/cc_transport_fcm.c
CC_CFLAGS += -DCC_TRANSPORT_FCM
endif

# C actors

ifeq ($(std.Identity),1)
CC_SRC_C += actors/cc_actor_identity.c
CC_CFLAGS += -DCC_ACTOR_IDENTITY
endif

ifeq ($(io.Light),1)
CC_SRC_C += actors/cc_actor_light.c
CC_CFLAGS += -DCC_ACTOR_LIGHT
endif

ifeq ($(io.Button),1)
CC_SRC_C += actors/cc_actor_button.c
CC_CFLAGS += -DCC_ACTOR_BUTTON
endif

ifeq ($(sensor.Temperature),1)
CC_SRC_C += actors/cc_actor_temperature.c
CC_CFLAGS += -DCC_ACTOR_TEMPERATURE
endif

ifeq ($(sensor.TriggeredTemperature),1)
CC_SRC_C += actors/cc_actor_triggered_temperature.c
CC_CFLAGS += -DCC_ACTOR_TRIGGERED_TEMPERATURE
endif

ifeq ($(sensor.TemperatureTagged),1)
CC_SRC_C += actors/cc_actor_temperature_tagged.c
CC_CFLAGS += -DCC_ACTOR_TEMPERATURE_TAGGED
endif

ifeq ($(context.RegistryAttribute),1)
CC_SRC_C += actors/cc_actor_registry_attribute.c
CC_CFLAGS += -DCC_ACTOR_REGISTRY_ATTIBUTE
endif

# MicroPython config
ifeq ($(MPY),1)
ifndef PYTHON_HEAP_SIZE
PYTHON_HEAP_SIZE = 25*1024
$(info PYTHON_HEAP_SIZE not set, using default $(PYTHON_HEAP_SIZE))
endif
ifndef PYTHON_STACK_SIZE
PYTHON_STACK_SIZE = 8*1024
$(info PYTHON_STACK_SIZE not set, using default $(PYTHON_STACK_SIZE))
endif
ifndef ACTOR_MODULES_DIR
ACTOR_MODULES_DIR=\""mpys/\""
$(info ACTOR_MODULES_DIR not set, using default $(ACTOR_MODULES_DIR))
endif
CC_LIBS += -lmicropython -lm
CC_LDFLAGS += -L$(CC_PATH)libmpy
CC_CFLAGS += -std=gnu99
CC_CFLAGS += -I$(CC_PATH)libmpy/build -I$(CC_PATH)micropython -I$(CC_PATH)libmpy
CC_CFLAGS += -DCC_PYTHON_ENABLED -DCC_STORAGE_ENABLED
CC_CFLAGS += -DCC_PYTHON_HEAP_SIZE=$(PYTHON_HEAP_SIZE) -DCC_PYTHON_STACK_SIZE=$(PYTHON_STACK_SIZE)
CC_CFLAGS += -DCC_ACTOR_MODULES_DIR=$(ACTOR_MODULES_DIR)
CC_SRC_C += actors/cc_actor_mpy.c libmpy/cc_mpy_port.c libmpy/cc_mpy_calvinsys.c
endif
