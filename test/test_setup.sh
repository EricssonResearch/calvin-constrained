#!/bin/bash

# Init submodules
git submodule init
git submodule update

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
pushd calvin-base
git fetch
git checkout 4765857f12652d5466714a6458b4c4d68db0c4b5
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
