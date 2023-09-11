#!/bin/bash

VERSION=$1

if [ -z "$VERSION" ]; then
  echo "usage: archive-win.sh <version>"
  exit
fi

function archive {
  ARCH=$1
  FILENAME="musikcube_${VERSION}_win${ARCH}"
  echo "Processing $FILENAME..."
  SRC_DIR="bin$ARCH"
  DST_DIR="dist/$VERSION/$FILENAME"
  rm -rf "$DST_DIR"
  mkdir -p "$DST_DIR/plugins"
  mkdir -p "$DST_DIR/themes"
  mkdir -p "$DST_DIR/locales"
  mkdir -p "$DST_DIR/fonts"
  cp $SRC_DIR/release/musikcube.exe           $DST_DIR
  cp $SRC_DIR/release/musikcube-cmd.exe       $DST_DIR
  cp $SRC_DIR/release/*.dll                   $DST_DIR
  cp $SRC_DIR/release/plugins/*.dll           $DST_DIR/plugins
  cp $SRC_DIR/release/themes/*.json           $DST_DIR/themes
  cp $SRC_DIR/release/locales/*.json          $DST_DIR/locales
  cp $SRC_DIR/release/fonts/*.ttf             $DST_DIR/fonts
  cp -rfp $SRC_DIR/release/plugins/Milkdrop2  $DST_DIR/plugins
  rm $DST_DIR/musikcore.dll 2> /dev/null
  pushd $DST_DIR
  7z a -tzip "$FILENAME.zip" ./* -mx=9
  mv "$FILENAME.zip" ..
  popd
}

archive "32"
archive "64"
