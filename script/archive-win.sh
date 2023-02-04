#!/bin/bash

VERSION=$1

if [ -z "$VERSION" ]; then
  echo "usage: archive-win.sh <version>"
  exit
fi

#
# 32-bit
#

WIN32="dist/musikcube_win32_$VERSION"
rm -rf "$WIN32"
mkdir -p "$WIN32/plugins"
mkdir -p "$WIN32/themes"
mkdir -p "$WIN32/locales"
mkdir -p "$WIN32/fonts"
cp bin32/release/musikcube.exe "$WIN32"
cp bin32/release/musikcube-cmd.exe "$WIN32"
cp bin32/release/*.dll "$WIN32"
cp bin32/release/plugins/*.dll "$WIN32/plugins"
cp bin32/release/themes/*.json "$WIN32/themes"
cp bin32/release/locales/*.json "$WIN32/locales"
cp bin32/release/fonts/*.ttf "$WIN32/fonts"
cp -rfp bin32/release/plugins/Milkdrop2 "$WIN32/plugins"
rm "$WIN32/musikcore.dll"
pushd $WIN32
7z a -tzip "musikcube_win32_$VERSION.zip" ./* -mx=9
mv "musikcube_win32_$VERSION.zip" ..
popd

#
# 64-bit
#

WIN64="dist/musikcube_win64_$VERSION"
rm -rf "$WIN64"
mkdir -p "$WIN64/plugins"
mkdir -p "$WIN64/themes"
mkdir -p "$WIN64/locales"
mkdir -p "$WIN64/fonts"
cp bin64/release/musikcube.exe "$WIN64"
cp bin64/release/musikcube-cmd.exe "$WIN64"
cp bin64/release/*.dll "$WIN64"
cp bin64/release/plugins/*.dll "$WIN64/plugins"
cp bin64/release/themes/*.json "$WIN64/themes"
cp bin64/release/locales/*.json "$WIN64/locales"
cp bin64/release/fonts/*.ttf "$WIN64/fonts"
cp -rfp bin64/release/plugins/Milkdrop2 "$WIN64/plugins"
rm "$WIN64/musikcore.dll"
pushd $WIN64
7z a -tzip "musikcube_win64_$VERSION.zip" ./* -mx=9
mv "musikcube_win64_$VERSION.zip" ..
popd
