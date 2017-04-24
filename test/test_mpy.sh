#!/bin/bash

rm calvinconstrained.config
rm cc_stderr.log
rm calvin_c

cp test/calvin.confLOCAL calvin.conf
PYTHONPATH=calvin-base python calvin-base/calvin/Tools/csruntime.py --name rt1 -n 127.0.0.1 -p 5000 -c 5001 &
RT1_PID=$!
PYTHONPATH=calvin-base python test/verify_runtime.py http://127.0.0.1:5001
exit_code+=$?
rm calvin.conf

cp test/calvin.confPROXY calvin.conf
PYTHONPATH=calvin-base python calvin-base/calvin/Tools/csruntime.py --name rt2 -n 127.0.0.1 -p 5002 -c 5003 &
RT2_PID=$!
PYTHONPATH=calvin-base python test/verify_runtime.py http://127.0.0.1:5003
exit_code+=$?
rm calvin.conf

# build and start calvin-constrained
make -f runtime/south/platform/x86/Makefile_mpy
exit_code+=$?

# run test
PYTHONPATH=calvin-base py.test -sv test/test.py
exit_code+=$?

mv cc_stderr.log cc_stderr_mpy.log

# clean up
kill -9 $RT1_PID
kill -9 $RT2_PID
cd calvin-base
git checkout calvin/csparser/parsetab.py

exit $exit_code
