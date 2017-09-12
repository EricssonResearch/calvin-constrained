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
git checkout 6359f99b185a31ad096f97392ddbe7cea578b8a2
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
