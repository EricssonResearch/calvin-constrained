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
git checkout a1dd35aad2c93c15b7e199c93283c8927e52f74b
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
