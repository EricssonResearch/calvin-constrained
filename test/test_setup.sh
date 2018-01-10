#!/bin/bash

# Init submodules
git submodule init
git submodule update

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
pushd calvin-base
git fetch
git checkout 57b174c840628fdd0d1d6c3f2a9e29cb199a65c5
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
