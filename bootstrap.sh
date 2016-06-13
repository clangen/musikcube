#!/bin/bash

rm CMakeCache.txt

rm -rf build
rm -rf bin

mkdir -p bin/plugins
mkdir build

mkdir build/release
cd build/release/
cmake -DCMAKE_BUILD_TYPE=Release ../../
cd ../../

mkdir build/debug
cd build/debug/
cmake -DCMAKE_BUILD_TYPE=Debug ../../
cd ../../
