# Python actors

The support to execute Python actors is built on [MicroPython](https://github.com/micropython/micropython) and by including it as a statically linked library when building the runtime.

Python actors are included in firmware by:

- Adding the actor module to the libmpy/modules folder, all modules included libmpy/modules folder are included in the firmware as frozen modules.
- Adding the actor compiled with mpy-cross compiler to the CC_ACTOR_MODULES_DIR folder on the filesystem.

If an actor isn't found in any of the above the constrained runtime will request the actor module from the calvin-base runtime acting as n proxy and write the actor to the CC_ACTOR_MODULES_DIR folder and load the pending actors.

Look at the runtime/south/platform/x86/Makefile_mpy for an example on how to include support for Python actors.
