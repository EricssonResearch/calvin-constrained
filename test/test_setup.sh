#!/bin/bash

# Init submodules
git submodule init
git submodule update

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
pushd calvin-base
git fetch
git checkout 1dbac431d4dea41bd556c61c70ddd489d228b5ab
pip install -r requirements.txt
pip install -r test-requirements.txt
popd
