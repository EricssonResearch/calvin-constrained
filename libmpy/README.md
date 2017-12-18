# Python actors

The support to execute Python actors is built on [MicroPython](https://github.com/micropython/micropython) and by including it as a statically linked library when building the runtime.

Build the library with:

```
$ make -C libmpy <target>
```

A MicroPython mpconfigport.h can be specified with the CONFIGFILE preprocessor define. Example:

```
make -C libmpy libarm CONFIGFILE="runtime/south/platform/spritzer/mpconfigport_spritzer.h"
```

And build the calvin-constrained binary with the MPY preprocessor define set to 1 to enable support for Python actors. Exampe:

```
make -f runtime/south/platform/x86/Makefile MPY=1 CONFIG="runtime/south/platform/x86/cc_config_x86_mpy.h"
```

Python modules and actors are included in the firmware by:

- Adding the module to the libmpy/modules folder. All modules included libmpy/modules folder are included in the firmware as frozen modules.
- Adding the actor compiled with mpy-cross compiler to the CC_ACTOR_MODULES_DIR folder on the filesystem.

If an actor isn't found in any of the above the constrained runtime will request the actor module from the calvin-base runtime acting as the proxy and write the actor to the CC_ACTOR_MODULES_DIR folder and load the pending actors.
