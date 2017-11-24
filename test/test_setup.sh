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
git checkout 327739b729babe2480c85e728df4a975612a42b7
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
