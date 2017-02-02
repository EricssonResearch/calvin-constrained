#!/bin/bash

# Init submodules
git submodule init
git submodule update

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
cd calvin-base
git checkout 3cbdefc00ae01ccbb00ad7d4ded448c429b62c78

pip install -r requirements.txt
pip install -r test-requirements.txt
