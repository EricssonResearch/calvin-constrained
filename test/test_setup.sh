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
git checkout 97af5eb755e8a944b4cd73b773e3463bc173d8fd
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
