#!/bin/bash

# setup and start calvin-base
git clone https://github.com/EricssonResearch/calvin-base.git
cd calvin-base
git fetch
git rebase
git checkout develop

virtualenv venv > /dev/null
source venv/bin/activate > /dev/null
pip install -q -r requirements.txt
pip install -q -r test-requirements.txt

PYTHONPATH=. python calvin/Tools/csruntime.py --name rt1 -n 127.0.0.1 -p 5000 -c 5001 &
RT1_PID=$!

# setup and start calvin-constrained
cd ..
make
./calvin_c &
CONSTRAINED_RT_PID=$!

# run test
PYTHONPATH=calvin-base py.test -sv test/test.py

# clean up
deactivate
kill -9 $RT1_PID
kill -9 $CONSTRAINED_RT_PID
cd calvin-base
git checkout calvin/csparser/parsetab.py
