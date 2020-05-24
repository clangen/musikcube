#!/bin/sh
DIR=$1
if [ -z "$DIR" ]; then
  echo "[strip] directory not specified, using current working directory"
  DIR=`pwd`
fi
echo "[strip] resolved directory: ${DIR}"
strip "$DIR/bin/musikcube"
strip "$DIR/bin/musikcubed"
find "$DIR/bin/" -name "*.so" -exec strip "{}" \; 2> /dev/null
echo "[strip] finished"
