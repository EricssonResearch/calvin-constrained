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
git checkout 6f48bbe87a92926dc15d9322cf437b1ac1dca2ba
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
