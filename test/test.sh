#!/bin/bash

PYTHONPATH=calvin-base python calvin-base/calvin/Tools/csruntime.py --name rt1 -n 127.0.0.1 -p 5000 -c 5001 &
RT1_PID=$!

PYTHONPATH=calvin-base python calvin-base/calvin/Tools/csruntime.py --name rt2 -n 127.0.0.1 -p 5002 -c 5003 &
RT2_PID=$!

PYTHONPATH=calvin-base python test/verify_registry.py

if [ $? -ne 0 ]; then
    echo "Failed to setup base runtimes"
else
  # build and start calvin-constrained
  make -f platform/x86/Makefile
  ./calvin_c -c 127.0.0.1 -d 5000 &
  CONSTRAINED_RT_PID=$!

  # run test
  PYTHONPATH=calvin-base py.test -sv test/test.py

  kill -9 $CONSTRAINED_RT_PID
fi

# clean up
kill -9 $RT1_PID
kill -9 $RT2_PID
cd calvin-base
git checkout calvin/csparser/parsetab.py
