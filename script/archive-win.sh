#!/bin/bash

set -e

VERSION=$1

if [ -z "$VERSION" ]; then
  echo "usage: archive-win.sh <version>"
  exit 1
fi

function archive {
  ARCH=$1
  FILENAME="musikcube_${VERSION}_win${ARCH}"
  echo "Processing $FILENAME..."
  SRC_DIR="bin64/Release"
  DST_DIR="dist/$VERSION/$FILENAME"
  rm -rf "$DST_DIR"
  mkdir -p "$DST_DIR/plugins"
  mkdir -p "$DST_DIR/themes"
  mkdir -p "$DST_DIR/locales"
  mkdir -p "$DST_DIR/fonts"
  cp $SRC_DIR/musikcube.exe           $DST_DIR
  cp $SRC_DIR/musikcube-cmd.exe       $DST_DIR
  cp $SRC_DIR/*.dll                   $DST_DIR
  cp $SRC_DIR/plugins/*.dll           $DST_DIR/plugins
  cp -rp "$SRC_DIR/plugins/projectM" "$DST_DIR/plugins"
  cp $SRC_DIR/themes/*.json           $DST_DIR/themes
  cp $SRC_DIR/locales/*.json          $DST_DIR/locales
  cp $SRC_DIR/fonts/*.ttf             $DST_DIR/fonts
  if [ -d "$SRC_DIR/plugins/Milkdrop2" ]; then
    cp -rp "$SRC_DIR/plugins/Milkdrop2" "$DST_DIR/plugins"
  fi
  pushd $DST_DIR
  7z a -tzip "$FILENAME.zip" ./* -mx=9
  mv "$FILENAME.zip" ..
  popd
}

archive "64"

SDK_NAME="musikcore_sdk_${VERSION}_win64"
SDK_DIR="dist/$VERSION/$SDK_NAME"
rm -rf "$SDK_DIR"
mkdir -p "$SDK_DIR"
cmake --install build/windows-x64 --config Release --component musikcoreSDK --prefix "$SDK_DIR"
pushd "$SDK_DIR"
7z a -tzip "$SDK_NAME.zip" ./* -mx=9
mv "$SDK_NAME.zip" ..
popd
