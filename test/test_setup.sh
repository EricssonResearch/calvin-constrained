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
git checkout 2b7e211baa9ee3fc36110457d545a5dbc764f635
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
