#!/bin/sh
DIR=$1
if [ -z "$DIR" ]; then
  echo "[strip-nix] directory not specified, using current working directory"
  DIR=`pwd`
fi
echo "[strip-nix] using directory: ${DIR}/bin/"
strip "$DIR/bin/*" 2> /dev/null
strip "$DIR/bin/lib/*" 2> /dev/null
strip "$DIR/bin/plugin/*" 2> /dev/null
echo "[strip-nix] finished"
