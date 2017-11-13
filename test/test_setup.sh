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
git checkout c3404e24e7543aa7150603869d5f75da3ca5bf05
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
