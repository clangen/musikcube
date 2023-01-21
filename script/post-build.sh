#!/bin/sh

CMAKE_CURRENT_SOURCE_DIR=$1
CMAKE_SYSTEM_NAME=$2
CMAKE_BUILD_TYPE=$3
BUILD_STANDALONE=$4
DISABLE_STRIP=$5

echo "[post-build] started..."

if [ "$CMAKE_BUILD_TYPE" = 'Release' ]; then
    echo "[post-build] BUILD_TYPE=${CMAKE_BUILD_TYPE}, stripping binaries"
    $CMAKE_CURRENT_SOURCE_DIR/script/strip-nix.sh $DIR
else
    echo "[post-build] BUILD_TYPE=${CMAKE_BUILD_TYPE}, not stripping"
fi

echo "[post-build] patching library rpath entries..."
$CMAKE_CURRENT_SOURCE_DIR/script/patch-rpath.sh $CMAKE_CURRENT_SOURCE_DIR

echo "[post-build] staging static assets..."
mkdir -p "$CMAKE_CURRENT_SOURCE_DIR/bin/themes"
cp -rfp "$CMAKE_CURRENT_SOURCE_DIR/src/musikcube/data/themes/"*.json "$CMAKE_CURRENT_SOURCE_DIR/bin/themes"

mkdir -p "$CMAKE_CURRENT_SOURCE_DIR/bin/locales"
cp -rfp "$CMAKE_CURRENT_SOURCE_DIR/src/musikcube/data/locales/"*.json "$CMAKE_CURRENT_SOURCE_DIR/bin/locales"

echo "[post-build] re-running 'cmake .' to re-index compiled artifacts"
cmake .

echo "[post-build] finished"
