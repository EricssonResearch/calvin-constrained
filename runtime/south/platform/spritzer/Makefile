-include $(TOPDIR)/Make.defs

CONFIG_CALVIN_PROGNAME ?= calvin$(EXEEXT)
PROGNAME = $(CONFIG_CALVIN_PROGNAME)

CONFIG_CALVIN_PRIORITY ?= SCHED_PRIORITY_DEFAULT
CONFIG_CALVIN_STACKSIZE ?= 2048
APPNAME = calvin
PRIORITY = $(CONFIG_CALVIN_PRIORITY)
STACKSIZE = $(CONFIG_CALVIN_STACKSIZE)

CC_PATH = $(APPDIR)/ericsson/calvin-constrained/

CC_SRC_C = runtime/south/platform/spritzer/cc_platform_spritzer.c \
	runtime/south/transport/spritzer/cc_transport_spritzer.c \
	runtime/south/platform/spritzer/calvinsys/cc_calvinsys_temp_sensor.c

CONFIG = runtime/south/platform/spritzer/cc_config_spritzer.h

include $(CC_PATH)calvin.mk

ASRCS =
MAINSRC = $(addprefix $(CC_PATH),$(CC_SRC_C))
CSRCS =
CFLAGS += -I$(APPDIR)/../nuttx/include -I$(CC_PATH) $(CC_CFLAGS)
LDFLAGS += $(CC_LDFLAGS)

include $(APPDIR)/Application.mk
