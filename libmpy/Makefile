
# Copyright (c) 2016 Ericsson AB
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

BUILD = build

# to reslove some location coflicts, we will use absolute paths
ABS_BUILD = $(abspath $(BUILD))

# set of folders needed by the makefile
## Top folder of Calvin constrained (from here)
CC_TOP = ..

# all remaining folders assume you are located in the calvin constrained root folder
## The compilation folder (current folder)
MPY_CONF_FOLDER = libmpy

## Top folder of Micropython
MP_TOP = micropython

## Frozen files (i.e. the Python actors) folder
###  Note: There should be at least one python file in this folder for the compilation to run
FROZEN_MPY_DIR = libmpy/modules

# Absolute path to frozen files (resolve some reference conflicts
ABS_FROZEN_MPY_DIR = $(abspath $(CC_TOP))/$(FROZEN_MPY_DIR)

# Add the names of the files for QSTR extraction
EXT_SRC_C = calvin_mpy_port.c calvin_mpy_gpiohandler.c calvin_mpy_environmental.c

# Add here the names of any additional c files to be compiled
ADD_C_SOURCE = 	$(addprefix $(MPY_CONF_FOLDER)/, $(EXT_SRC_C)) actors/actor_mpy.c

# The -DMICROPYTHON enables the MICROPYTHON directive in the program
CFLAGS = -std=c99 -I$(MPY_CONF_FOLDER) -I$(MP_TOP) -DMICROPYTHON $(CFLAGS_EXTRA) -Os -DNDEBUG

# Library location and library filename (to be passed on to the application)
LDFLAGS = -L$(MPY_CONF_FOLDER) -lmicropython -lm

lib:
	@echo "Making micropython library for x86"
	@$(MAKE) clean && $(MAKE) -f Makefile.upylib MPTOP=$(CC_TOP)/$(MP_TOP) FROZEN_MPY_DIR=$(CC_TOP)/$(FROZEN_MPY_DIR) DEBUG=DEBUG EXT_SRC_C="$(EXT_SRC_C)"

libarm:
	@echo "Making micropython library for ARM"
	@$(MAKE) clean && $(MAKE) -f Makefile.upylib.arm MPTOP=$(CC_TOP)/$(MP_TOP) FROZEN_MPY_DIR=$(CC_TOP)/$(FROZEN_MPY_DIR) DEBUG=DEBUG EXT_SRC_C="$(EXT_SRC_C)"

clean:
	@rm -rf *.a
	@rm -rf build libmpy micropython