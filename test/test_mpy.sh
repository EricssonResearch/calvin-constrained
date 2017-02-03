#!/bin/bash

cp test/calvin.confLOCAL calvin.conf
PYTHONPATH=calvin-base python calvin-base/calvin/Tools/csruntime.py --name rt1 -n 127.0.0.1 -p 5000 -c 5001 &
RT1_PID=$!
PYTHONPATH=calvin-base python test/verify_runtime.py http://127.0.0.1:5001
rm calvin.conf

if [ $? -ne 0 ]; then
    echo "Failed to setup base runtime 1"
    kill -9 $RT1_PID
    exit
fi

cp test/calvin.confPROXY calvin.conf
PYTHONPATH=calvin-base python calvin-base/calvin/Tools/csruntime.py --name rt2 -n 127.0.0.1 -p 5002 -c 5003 &
RT2_PID=$!
PYTHONPATH=calvin-base python test/verify_runtime.py http://127.0.0.1:5003
rm calvin.conf

if [ $? -ne 0 ]; then
    echo "Failed to setup base runtime 2"
    kill -9 $RT1_PID
    kill -9 $RT2_PID
    exit
fi

# build and start calvin-constrained
make -f platform/x86/Makefile_mpy
./calvin_c_mpy -n constrained -i 127.0.0.1 -p 5000 &
CONSTRAINED_RT_PID=$!

# run test
PYTHONPATH=calvin-base py.test -sv test/test.py

# clean up
kill -9 $CONSTRAINED_RT_PID
kill -9 $RT1_PID
kill -9 $RT2_PID
cd calvin-base
git checkout calvin/csparser/parsetab.py
