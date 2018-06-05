#!/bin/bash

# Init submodules
git submodule init
git submodule update

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
pushd calvin-base
git fetch
git checkout 7e60f8f364fd27a92741a1c0a0fbd0759e7d5294
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
