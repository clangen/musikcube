#!/bin/sh
make clean 2> /dev/null
rm -rf bin
find . -name CMakeCache.txt -delete
find . -name CMakeFiles -type d -exec rm -rf "{}" \; 2> /dev/null
rm -f Makefile
rm -f *.cmake
rm -rf _CPack_Packages 2> /dev/null
rm CPack* 2> /dev/null
rm install_manifest.txt 2> /dev/null