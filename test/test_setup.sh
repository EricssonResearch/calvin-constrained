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
git checkout 78aa6b4173f33d31740cf6d4c60da0fd35d4b008
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
