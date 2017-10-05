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
git checkout 6d28728df0cafeb5500943059a6061b925fe3741
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
