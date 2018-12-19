#!/bin/bash

while getopts :p:em opt; do
  case $opt in
    e) USE_PREBUILT=1
      ;;
    m) MPY=1
      ;;
    p) PP=${OPTARG}
      ;;
    \?)
      usage
      echo "Invalid option: -${OPTARG}"
      ;;
  esac
done

# clean up
if [ -f calvin.msgpack ]; then
  rm calvin.msgpack
fi

if [ -f calvin_c ]; then
  rm calvin_c
fi

if [ -d mpys ]; then
  rm -rf mpys
fi

if [ ! $PP ]; then
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

if [ $USE_PREBUILT ]; then
  echo "Use prebuilt"
else
  if [ $MPY ]; then
    echo "Building with MicroPython"
    make -C libmpy
    make -f runtime/south/platform/x86/Makefile MPY=1 CONFIG="runtime/south/platform/x86/cc_config_x86_test.h"
  else
    echo "Building without MicroPython"
    make -f runtime/south/platform/x86/Makefile CONFIG="runtime/south/platform/x86/cc_config_x86_test.h"
  fi
fi

exit_code+=$?

# run test
PYTHONPATH=$PP py.test -p no:twisted -sv test/test.py
exit_code+=$?

# clean up
kill -9 $RT1_PID
kill -9 $RT2_PID
if [[ ${CHECKOUT} == "yes" ]]; then
  pushd calvin-base
  git checkout calvin/csparser/parsetab.py
  popd
fi

exit $exit_code
