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
git checkout a3ec77fbc965ad4355f02a486f5fae71aa6869e5
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
