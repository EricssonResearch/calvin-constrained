#!/bin/bash

# start first calvin-base runtime
PYTHONPATH=calvin-base python calvin-base/calvin/Tools/csruntime.py --name rt1 -n 127.0.0.1 -p 5000 -c 5001 &
RT1_PID=$!

# build and start calvin-constrained
make -f platform/x86/Makefile_mpy
./calvin_c_mpy -c 127.0.0.1 -d 5000 &
CONSTRAINED_RT_PID=$!

# run test
PYTHONPATH=calvin-base py.test -sv test/test.py

# clean up
kill -9 $RT1_PID
kill -9 $CONSTRAINED_RT_PID
cd calvin-base
git checkout calvin/csparser/parsetab.py
