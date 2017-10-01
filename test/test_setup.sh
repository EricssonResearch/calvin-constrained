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
git checkout c2718e32b38878ea2ded035ba8d02a7c55e72dbc
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
