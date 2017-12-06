-include $(TOPDIR)/Make.defs

CONFIG_CALVIN_PROGNAME ?= calvin$(EXEEXT)
PROGNAME = $(CONFIG_CALVIN_PROGNAME)

CONFIG_CALVIN_PRIORITY ?= SCHED_PRIORITY_DEFAULT
CONFIG_CALVIN_STACKSIZE ?= 2048
APPNAME = calvin
PRIORITY = $(CONFIG_CALVIN_PRIORITY)
STACKSIZE = $(CONFIG_CALVIN_STACKSIZE)

CC_PATH = $(APPDIR)/ericsson/calvin-constrained/

SPRITZER = 1

CC_SRC_C = runtime/south/platform/spritzer/cc_platform_spritzer.c \
	runtime/south/transport/spritzer/cc_transport_spritzer.c \
	runtime/south/platform/spritzer/calvinsys/cc_calvinsys_temp_sensor.c

ifdef CONFIG_CALVIN_INACTIVITY_TIMEOUT
INACTIVITY_TIMEOUT = $(CONFIG_CALVIN_INACTIVITY_TIMEOUT)
endif

ifeq ($(CONFIG_CALVIN_STORAGE),y)
STORAGE = 1
CONFIG_FILE = CONFIG_CALVIN_CONFIG_FILE
endif

ifeq ($(CONFIG_CALVIN_PYTHON),y)
MPY=1
ifdef CONFIG_CALVIN_PYTHON_HEAPSIZE
PYTHON_HEAP_SIZE = $(CONFIG_CALVIN_PYTHON_HEAPSIZE)
endif
ifdef CONFIG_CALVIN_PYTHON_STACKSIZE
PYTHON_STACK_SIZE = $(CONFIG_CALVIN_PYTHON_STACKSIZE)
endif
ifdef CONFIG_CALVIN_ACTOR_MODULES_DIR
ACTOR_MODULES_DIR = $(CONFIG_CALVIN_ACTOR_MODULES_DIR)
endif
endif

ifeq ($(CONFIG_CALVIN_SLEEP),y)
SLEEP = 1
ifdef CONFIG_CALVIN_SLEEP_TIME
SLEEP_TIME = $(CONFIG_CALVIN_SLEEP_TIME)
endif
endif

ifeq ($(CONFIG_CALVIN_CACTORS),y)
ifeq ($(CONFIG_CALVIN_STD_IDENTITY),y)
std.Identity=1
endif
ifeq ($(CONFIG_CALVIN_IO_LIGHT),y)
io.Light=1
endif
ifeq ($(CONFIG_CALVIN_IO_BUTTON),y)
io.Button=1
endif
ifeq ($(CONFIG_CALVIN_SENSOR_TEMPERATURE),y)
sensor.Temperature=1
endif
ifeq ($(CONFIG_CALVIN_SENSOR_TRIGGERED_TEMPERATURE),y)
sensor.TriggeredTemperature=1
endif
ifeq ($(CONFIG_CALVIN_CONTEXT_REGISTRYATTRIBUTE),y)
context.RegistryAttribute=1
endif
endif

include $(CC_PATH)calvin.mk

ASRCS =
MAINSRC = $(addprefix $(CC_PATH),$(CC_SRC_C))
CSRCS =
CFLAGS += -I$(APPDIR)/../nuttx/include -I$(CC_PATH) $(CC_CFLAGS)
LDFLAGS += $(CC_LDFLAGS)

include $(APPDIR)/Application.mk
