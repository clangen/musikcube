#!/bin/sh

VERSION=$1

if [ -z "$VERSION" ]; then
  echo "usage: ./archive-win32.sh <version>"
  exit
fi

VANILLA="bin/dist/musikcube_win32_$VERSION"
MILKDROP="bin/dist/musikcube_with_milkdrop2_win32_$VERSION"

rm -rf "$VANILLA"
rm -rf "$MILKDROP"

mkdir -p "$VANILLA/plugins"
cp bin/release/musikbox.exe "$VANILLA" 
cp bin/release/plugins/*.dll "$VANILLA/plugins"
rm "$VANILLA/plugins/vis_milk2.dll"

mkdir -p "$MILKDROP/plugins"
cp bin/release/musikbox.exe "$MILKDROP"
cp bin/release/plugins/*.dll "$MILKDROP/plugins"
cp -rfp bin/release/plugins/Milkdrop2 "$MILKDROP/plugins"
