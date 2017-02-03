#!/bin/bash

# Init submodules
git submodule init
git submodule update

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
cd calvin-base
git fetch
git checkout 1760e34829ac0f597d52b38ab9e7f5683cab31b0
pip install -r requirements.txt
pip install -r test-requirements.txt
