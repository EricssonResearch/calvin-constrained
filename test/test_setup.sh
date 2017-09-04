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
git checkout acaa569767e4b162d59003e393eeb5166fe64c0b
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
