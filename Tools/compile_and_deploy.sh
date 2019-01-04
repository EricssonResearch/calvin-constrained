#!/bin/bash
SCRIPT=$1
FILE=${SCRIPT%.*}
COMPILED=${FILE}.json
CODED=${FILE}.msgpack

if [ ! -f calvin_c ]; then
  make -C libmpy
  make -f runtime/south/platform/x86/Makefile CONFIG="runtime/south/platform/x86/cc_config_x86_mpy.h" MPY=1
fi

PYTHONPATH=calvin-base python calvin-base/calvin/Tools/cscompiler.py $SCRIPT

python Tools/json2msgpack.py $COMPILED > $CODED

./calvin_c -s $CODED -u '["calvinip://127.0.0.1:5000", "ssdp"]' -a '{"indexed_public": {"node_name": {"name": "calvin_klein"}}}'
