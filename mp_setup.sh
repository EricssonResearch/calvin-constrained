  #
 # Copyright (c) 2016 Ericsson AB
 #
 # Licensed under the Apache License, Version 2.0 (the "License");
 # you may not use this file except in compliance with the License.
 # You may obtain a copy of the License at
 #
 #    http://www.apache.org/licenses/LICENSE-2.0
 #
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
 #
 # Bash script to set up Calvin-constrained with Micropython to work for the first time.
 # Basically, this script initializes and updates the git submodules both in MP and CC
 # Then, it installes needed dependencies for MP. and compiles needed submodule files.
 # Finally, it compiles the mpy-cross tool used to generate frozen files.

echo "|---------------------------------|"
echo "|   Setting up the environment    |"
echo "|---------------------------------|"
echo
echo
echo " [1]---- initializing and updating GIT modules in calvin-constrained"
git submodule update --init

echo
echo
echo " [2]---- installing necessary packages needed by Micropython (sudo rights needed)"
sudo apt-get install build-essential libffi-dev pkg-config libtool g++-multilib

echo
echo
echo " [3]---- initializing, updating and compiling GIT modules in Micropython "
cd micropython
git reset --hard bc4441afa753176e1901017708d8a739c38806eb
cd unix
make deplibs

echo
echo
echo " [4]---- compiling mpy-cross tool "
cd ../mpy-cross
make
cd ../..
