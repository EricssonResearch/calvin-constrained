#!/bin/bash

# setup and start calvin-base
git clone https://github.com/EricssonResearch/calvin-base.git
cd calvin-base
git checkout develop
git fetch
git rebase

virtualenv venv > /dev/null
source venv/bin/activate > /dev/null
pip install -q -r requirements.txt
pip install -q -r test-requirements.txt

PYTHONPATH=. python calvin/Tools/csruntime.py --name rt1 -n 127.0.0.1 -p 5000 -c 5001 &
RT1_PID=$!

# setup and start calvin-constrained
cd ..
if [ $# -eq 1 ]
then
    if [ $1 == "mpy" ]
    then
        make -f platform/x86/Makefile_mpy
        ./calvin_c_mpy -c 127.0.0.1 -d 5000 &
        CONSTRAINED_RT_PID=$!

        # run test
        PYTHONPATH=calvin-base py.test -sv test/test.py
    else
        echo "Unknown arg"
    fi
else
    make -f platform/x86/Makefile
    ./calvin_c -c 127.0.0.1 -d 5000 &
    CONSTRAINED_RT_PID=$!

    # run test
    PYTHONPATH=calvin-base py.test -sv test/test.py
fi

# clean up
deactivate
kill -9 $RT1_PID
kill -9 $CONSTRAINED_RT_PID
cd calvin-base
git checkout calvin/csparser/parsetab.py
