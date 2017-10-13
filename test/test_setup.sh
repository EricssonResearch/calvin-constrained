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
git checkout 7878fe0fdb050f32e1c122dabf389536a5f72e18
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
