#!/bin/bash

# Init submodules
git submodule init
git submodule update

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
pushd calvin-base
git fetch
git checkout 4805455708179132f17b66c10b03f0f7b3940a97
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
