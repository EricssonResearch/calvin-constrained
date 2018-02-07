#!/bin/bash

# Init submodules
git submodule init
git submodule update

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
pushd calvin-base
git fetch
git checkout daab087ae7eb76be0d71b47ef04701fecd3778e9
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
