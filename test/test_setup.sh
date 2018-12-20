#!/bin/bash

# Init submodules
git submodule init
git submodule update

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
pushd calvin-base
git fetch
git checkout ec62de3b442bfb96406b9f54bba6bc6c00f4cd7e
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
