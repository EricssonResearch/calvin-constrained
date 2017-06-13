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
git checkout df947239e084f3b173f938efc947a500b813e8ad
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
