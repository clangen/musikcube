#!/bin/sh
strip ./bin/musikcube
strip ./bin/musikcubed
find . -name "*.so" -exec strip "{}" \; 2> /dev/null
