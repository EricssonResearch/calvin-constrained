#!/bin/bash

# Init submodules
git submodule init
git submodule update

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
pushd calvin-base
git fetch
git checkout afcc131b9d78bedcd3f59c622432ce39f41d73fe
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
