#!/bin/bash

ROOT=$(pwd)

# build googletest
GTEST_FILE="third_party/googletest/build/googlemock/gtest/libgtest.a"
if [ ! -f "$GTEST_FILE" ]
then
	cd third_party/googletest
	export GTEST_TARGET=googlemock
	./travis.sh
	cd $ROOT
fi

