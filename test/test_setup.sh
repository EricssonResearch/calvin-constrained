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
git checkout 14fdbf4251fd0f5121dc1b26b8c7ae733aa4f912
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
