#!/bin/bash
SCRIPT=$1
FILE=${SCRIPT%.*}
COMPILED=${FILE}.json
CODED=${FILE}.msgpack

make -C libmpy clean
make -f runtime/south/platform/x86/Makefile clean
make -C libmpy
make -f runtime/south/platform/x86/Makefile CONFIG="calvin_scripts/cc_config_script.h" MPY=1

PYTHONPATH=calvin-base python calvin-base/calvin/Tools/cscompiler.py $SCRIPT

python Tools/json2msgpack.py $COMPILED > $CODED

#valgrind ./calvin_c -s $CODED -u '["calvinip://127.0.0.1:5000", "ssdp"]' -a '{"indexed_public": {"node_name": {"name": "calvin_klein"}}}'
./calvin_c -s $CODED -a '{"indexed_public": {"node_name": {"name": "calvin_klein"}}}'
