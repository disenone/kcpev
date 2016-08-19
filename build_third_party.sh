#!/bin/bash

ROOT=$(pwd)

# build googletest
GTEST_FILE="third_party/googletest/build/googlemock/gtest/libgtest.a"
if [ ! -f "$GTEST_FILE" ]
then
	git submodule update --init --recursive
	cd third_party/googletest
	./travis.sh
	mkdir build || true
	cd build
	cmake ..
	make
	cd $ROOT
fi

