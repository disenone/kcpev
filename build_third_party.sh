#!/bin/bash

ROOT=$(pwd)

git submodule update --init --recursive

# build googletest
GTEST_FILE="third_party/googletest/build/googlemock/gtest/libgtest.a"
if [ ! -f "$GTEST_FILE" ]
then
	cd third_party/googletest
	mkdir build || true
	cd build
	cmake ..
	make
	cd $ROOT
fi

# build libut
#UT_FILE="third_party/uthash/libut/libut.so"
#if [ ! -f "$UT_FILE" ]
#then
    #cp utMakefile third_party/uthash/libut/Makefile
	#cd third_party/uthash/libut
    #make clean
	#make
    #git checkout Makefile
	#cd $ROOT
#fi

