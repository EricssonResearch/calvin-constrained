ifdef CONFIG
CC_CFLAGS += -DCC_CONFIGFILE_H=\""$(CONFIG)\""
endif

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
	calvinsys/common/cc_calvinsys_attribute.c \
	jsmn/jsmn.c

# Prefix with CC_PATH
CC_SRC_C := $(addprefix $(CC_PATH),$(CC_SRC_C))

# C actors with CC_PATH prefix
actor_dirs := $(wildcard $(CC_PATH)actors/*/)
CC_SRC_C += $(wildcard $(addsuffix *.c,$(actor_dirs)))

# MicroPython config
ifeq ($(MPY),1)
CC_LIBS += -lmicropython -lm
CC_LDFLAGS += -L$(CC_PATH)libmpy
CC_CFLAGS += -DCC_USE_PYTHON=1 -std=gnu99
CC_CFLAGS += -I$(CC_PATH)libmpy/build -I$(CC_PATH)micropython -I$(CC_PATH)libmpy
CC_SRC_C += $(addprefix $(CC_PATH),libmpy/cc_actor_mpy.c libmpy/cc_mpy_port.c libmpy/cc_mpy_calvinsys.c)
endif
