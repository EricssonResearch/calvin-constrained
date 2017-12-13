#!/bin/bash

# clean up
rm calvin.msgpack
rm calvin_c
rm -rf mpys

# start base rt1
cp test/calvin.confLOCAL calvin.conf
PYTHONPATH=calvin-base python calvin-base/calvin/Tools/csruntime.py --name base_rt1 -n 127.0.0.1 -p 5000 -c 5001 &
RT1_PID=$!
PYTHONPATH=calvin-base python test/verify_runtime.py http://127.0.0.1:5001
exit_code+=$?
rm calvin.conf

# start base rt2 with rt1 as storage proxy
cp test/calvin.confPROXY calvin.conf
PYTHONPATH=calvin-base python calvin-base/calvin/Tools/csruntime.py --name base_rt2 -n 127.0.0.1 -p 5002 -c 5003 &
RT2_PID=$!
PYTHONPATH=calvin-base python test/verify_runtime.py http://127.0.0.1:5003
exit_code+=$?
rm calvin.conf

# build calvin-constrained
if [[ $1 == "mpy" ]]; then
    make -f runtime/south/platform/x86/Makefile mpy MPY=1 CONFIG="runtime/south/platform/x86/cc_config_x86_mpy.h"
else
    make -f runtime/south/platform/x86/Makefile CONFIG="runtime/south/platform/x86/cc_config_x86.h"
fi

exit_code+=$?

# run test
PYTHONPATH=calvin-base py.test -sv test/test.py
exit_code+=$?

# clean up
kill -9 $RT1_PID
kill -9 $RT2_PID
cd calvin-base
git checkout calvin/csparser/parsetab.py

exit $exit_code
