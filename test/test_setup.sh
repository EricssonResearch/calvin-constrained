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
git checkout dc1ecdc802d432ab33052c7fa0adb35595fdc41a
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
