#!/bin/sh
DIR=$1
if [ -z "$DIR" ]; then
  echo "[strip] directory not specified, using current working directory"
  DIR=`pwd`
fi
echo "[strip] resolved directory: ${DIR}"
strip "$DIR/bin/*" 2> /dev/null
strip "$DIR/bin/lib/*" 2> /dev/null
strip "$DIR/bin/plugin/*" 2> /dev/null
echo "[strip] finished"
