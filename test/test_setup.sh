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
git checkout bb2f4d7c8da6cb479817b7a43b750c34ffbe06ef
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
