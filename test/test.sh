#!/bin/bash

# clean up
rm calvin.msgpack
if [[ $1 != "no" ]]; then
	rm calvin_c
	rm -rf mpys
fi
if [ -z "${PP}" ]; then
	CHECKOUT="no"
	PP='calvin-base'
else
	CHECKOUT="yes"
fi

# start base rt1
cp test/calvin.confLOCAL calvin.conf
PYTHONPATH=$PP python $PP/calvin/Tools/csruntime.py -n 127.0.0.1 -p 5000 -c 5001 --attr "{\"indexed_public\": {\"node_name\": {\"organization\": \"com.ericsson\", \"purpose\": \"distributed-test\", \"group\": \"first\", \"name\": \"runtime1\"}}}" &
RT1_PID=$!
PYTHONPATH=$PP python test/verify_runtime.py http://127.0.0.1:5001
exit_code+=$?
rm calvin.conf

# start base rt2 with rt1 as storage proxy
cp test/calvin.confPROXY calvin.conf
PYTHONPATH=$PP python $PP/calvin/Tools/csruntime.py -n 127.0.0.1 -p 5002 -c 5003 --attr "{\"indexed_public\": {\"node_name\": {\"organization\": \"com.ericsson\", \"purpose\": \"distributed-test\", \"group\": \"rest\", \"name\": \"runtime2\"}}}" &
RT2_PID=$!
PYTHONPATH=$PP python test/verify_runtime.py http://127.0.0.1:5003
exit_code+=$?
rm calvin.conf

# build calvin-constrained
if [[ $1 == "mpy" ]]; then
    make -C libmpy
    make -f runtime/south/platform/x86/Makefile MPY=1 CONFIG="runtime/south/platform/x86/cc_config_x86_test.h"
elif [[ $1 == "no" ]]; then
	echo "Use prebuilt"
else
    make -f runtime/south/platform/x86/Makefile CONFIG="runtime/south/platform/x86/cc_config_x86_test.h"
fi

exit_code+=$?

# run test
PYTHONPATH=$PP py.test -p no:twisted -sv test/test.py test/test_replication.py
exit_code+=$?

# clean up
kill -9 $RT1_PID
kill -9 $RT2_PID
if [[ ${CHECKOUT} == "yes" ]]; then
	cd calvin-base
	git checkout calvin/csparser/parsetab.py
fi

exit $exit_code
