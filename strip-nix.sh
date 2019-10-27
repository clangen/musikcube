#!/bin/sh
strip ./bin/musikcube
strip ./bin/musikcubed
find ./bin/ -name "*.so" -exec strip "{}" \; 2> /dev/null
