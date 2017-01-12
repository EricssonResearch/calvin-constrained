#!/bin/bash

# fetch calvin-base and install required packages
git clone https://github.com/EricssonResearch/calvin-base.git
cd calvin-base
git checkout 161da6401df210e84693808eebd8ab68cabc41a3

pip install -r requirements.txt
pip install -r test-requirements.txt
