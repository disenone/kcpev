#!/bin/bash

# build third_party
./build_third_party.sh

mkdir build || true
cd build
cmake ..
make

