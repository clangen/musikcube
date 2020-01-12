#!/bin/sh
make clean 2> /dev/null
rm -rf bin
find . -name CMakeCache.txt -delete
find . -name CMakeFiles -type d -exec rm -rf "{}" \; 2> /dev/null
