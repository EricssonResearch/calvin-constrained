-include $(TOPDIR)/Make.defs

CC_PATH = $(APPDIR)/ericsson/calvin-constrained

CONFIG_CALVIN_PRIORITY ?= SCHED_PRIORITY_DEFAULT
CONFIG_CALVIN_STACKSIZE ?= 2048

APPNAME = calvin
PRIORITY = $(CONFIG_CALVIN_PRIORITY)
STACKSIZE = $(CONFIG_CALVIN_STACKSIZE)

CFLAGS += -I$(CC_PATH)/
CFLAGS += -I$(CC_PATH)/msgpuck
CFLAGS += -I$(CC_PATH)/calvinsys
CFLAGS += -I$(CC_PATH)/calvinsys/common
CFLAGS += -I$(CC_PATH)/runtime/north
CFLAGS += -I$(CC_PATH)/runtime/north/coder
CFLAGS += -I$(CC_PATH)/runtime/north/scheduler/np_scheduler
CFLAGS += -I$(CC_PATH)/runtime/south/transport/spritzer
CFLAGS += -I$(CC_PATH)/runtime/south/transport/spritzer/calvinsys
CFLAGS += -I$(CC_PATH)/actors

CFLAGS += -DCC_TRANSPORT_SPRITZER
#CFLAGS += -DCC_STORAGE_ENABLED -DCC_DEEPSLEEP_ENABLED -DCC_SLEEP_TIME=60 -DCC_RUNTIME_STATE_BUFFER_SIZE=10000
CFLAGS += -DCC_ACTOR_IDENTITY -DCC_ACTOR_TEMPERATURE
CFLAGS += -DCC_ADD_STRNSTR

ASRCS =
MAINSRC = $(CC_PATH)/runtime/south/platform/spritzer/cc_platform_spritzer.c \
$(CC_PATH)/cc_api.c \
$(CC_PATH)/runtime/north/cc_common.c \
$(CC_PATH)/runtime/north/scheduler/np_scheduler/cc_scheduler.c \
$(CC_PATH)/runtime/north/cc_node.c \
$(CC_PATH)/runtime/north/cc_proto.c \
$(CC_PATH)/runtime/south/transport/spritzer/cc_transport_spritzer.c \
$(CC_PATH)/runtime/north/cc_transport.c \
$(CC_PATH)/runtime/north/cc_tunnel.c \
$(CC_PATH)/runtime/north/cc_link.c \
$(CC_PATH)/runtime/north/cc_actor_store.c \
$(CC_PATH)/runtime/north/cc_actor.c \
$(CC_PATH)/runtime/north/cc_port.c \
$(CC_PATH)/runtime/north/cc_fifo.c \
$(CC_PATH)/runtime/north/cc_token.c \
$(CC_PATH)/runtime/north/coder/cc_coder_msgpuck.c \
$(CC_PATH)/msgpuck/msgpuck.c \
$(CC_PATH)/calvinsys/cc_calvinsys.c \
$(CC_PATH)/calvinsys/common/cc_calvinsys_timer.c \
$(CC_PATH)/calvinsys/common/cc_calvinsys_attribute.c \
$(CC_PATH)/actors/cc_actor_identity.c \
$(CC_PATH)/actors/cc_actor_temperature.c \
$(CC_PATH)/actors/cc_actor_registry_attribute.c \
$(CC_PATH)/runtime/south/platform/spritzer/calvinsys/cc_calvinsys_temp_sensor.c
CSRCS =

CONFIG_CALVIN_PROGNAME ?= calvin$(EXEEXT)
PROGNAME = $(CONFIG_CALVIN_PROGNAME)

include $(APPDIR)/Application.mk
