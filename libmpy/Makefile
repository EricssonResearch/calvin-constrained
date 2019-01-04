
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

# set of folders needed by the makefile
## Top folder of Calvin constrained (from here)
CC_TOP = ..

## Top folder of Micropython
MP_TOP = micropython

## Frozen files (i.e. the Python actors) folder
###  Note: There should be at least one python file in this folder for the compilation to run
FROZEN_MPY_DIR = libmpy/modules

# Add the names of the files for QSTR extraction
EXT_SRC_C = cc_mpy_port.c cc_mpy_calvinsys.c cc_mpy_socket.c

lib:
	@echo "Making micropython library for x86"
	$(MAKE) -f Makefile.upylib MPTOP=$(CC_TOP)/$(MP_TOP) FROZEN_MPY_DIR=$(CC_TOP)/$(FROZEN_MPY_DIR) DEBUG=DEBUG EXT_SRC_C="$(EXT_SRC_C)"

esp8266:
	@echo "Making micropython library for esp8266"
	$(MAKE) -f Makefile.upylib.esp8266 MPTOP=$(CC_TOP)/$(MP_TOP) FROZEN_MPY_DIR=$(CC_TOP)/$(FROZEN_MPY_DIR) DEBUG=DEBUG EXT_SRC_C="$(EXT_SRC_C)"

android:
	@echo "Making micropython library for android"
	$(MAKE) -f Makefile.upylib.android MPTOP=$(CC_TOP)/$(MP_TOP) FROZEN_MPY_DIR=$(CC_TOP)/$(FROZEN_MPY_DIR) DEBUG=DEBUG EXT_SRC_C="$(EXT_SRC_C)"

libarm:
	@echo "Making micropython library for ARM"
	$(MAKE) -f Makefile.upylib.arm MPTOP=$(CC_TOP)/$(MP_TOP) CONFIGFILE=$(CONFIGFILE) FROZEN_MPY_DIR=$(CC_TOP)/$(FROZEN_MPY_DIR) DEBUG=DEBUG EXT_SRC_C="$(EXT_SRC_C)"

clean:
	@rm -rf *.a
	@rm -rf build libmpy micropython
