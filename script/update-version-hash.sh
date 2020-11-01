#!/bin/sh

COMMIT_HASH=`git rev-parse --short HEAD 2> /dev/null | sed "s/\(.*\)/#\1/"`
sed -Ei.bak "s/(\s*)(#define VERSION_COMMIT_HASH )(.*)/\1\2\"${COMMIT_HASH}\"/g" src/musikcore/version.h
rm src/musikcore/version.h.bak
