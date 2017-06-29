#!/bin/bash

pushd ../
git clone https://github.com/PatrikAAberg/dmce.git
popd

# Init submodules
git submodule init
git submodule update

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
pushd calvin-base
git fetch
git checkout 9a0343cadc0c90a2c13ded66b2ea8953ffa24b0b
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
