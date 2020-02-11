#!/bin/sh

VERSION=$1

if [ -z "$VERSION" ]; then
  echo "usage: ./archive-win.sh <version>"
  exit
fi

#
# 32-bit
#

VANILLA="dist/musikcube_win32_$VERSION"
MILKDROP="dist/musikcube_win32_with_milkdrop2_$VERSION"

rm -rf "$VANILLA"
rm -rf "$MILKDROP"

mkdir -p "$VANILLA/plugins"
mkdir -p "$VANILLA/themes"
mkdir -p "$VANILLA/locales"
mkdir -p "$VANILLA/fonts"
cp bin32/release/musikcube.exe "$VANILLA"
cp bin32/release/musikcube-cmd.exe "$VANILLA"
cp bin32/release/*.dll "$VANILLA"
cp bin32/release/plugins/*.dll "$VANILLA/plugins"
cp bin32/release/themes/*.json "$VANILLA/themes"
cp bin32/release/locales/*.json "$VANILLA/locales"
cp bin32/release/fonts/*.ttf "$VANILLA/fonts"
rm "$VANILLA/plugins/vis_milk2.dll"
rm "$VANILLA/musikcore.dll"
pushd $VANILLA
7z a -tzip "musikcube_win32_$VERSION.zip" ./* -mx=9
mv "musikcube_win32_$VERSION.zip" ..
popd

mkdir -p "$MILKDROP/plugins"
mkdir -p "$MILKDROP/themes"
mkdir -p "$MILKDROP/locales"
mkdir -p "$MILKDROP/fonts"
cp bin32/release/musikcube.exe "$MILKDROP"
cp bin32/release/musikcube-cmd.exe "$MILKDROP"
cp bin32/release/*.dll "$MILKDROP"
cp bin32/release/plugins/*.dll "$MILKDROP/plugins"
cp bin32/release/themes/*.json "$MILKDROP/themes"
cp bin32/release/locales/*.json "$MILKDROP/locales"
cp bin32/release/fonts/*.ttf "$MILKDROP/fonts"
cp -rfp bin32/release/plugins/Milkdrop2 "$MILKDROP/plugins"
rm "$MILKDROP/musikcore.dll"
pushd $MILKDROP
7z a -tzip "musikcube_win32_with_milkdrop2_$VERSION.zip" ./* -mx=9
mv "musikcube_win32_with_milkdrop2_$VERSION.zip" ..
popd

#
# 64-bit
#

VANILLA="dist/musikcube_win64_$VERSION"

rm -rf "$VANILLA"

mkdir -p "$VANILLA/plugins"
mkdir -p "$VANILLA/themes"
mkdir -p "$VANILLA/locales"
mkdir -p "$VANILLA/fonts"
cp bin64/release/musikcube.exe "$VANILLA"
cp bin64/release/musikcube-cmd.exe "$VANILLA"
cp bin64/release/*.dll "$VANILLA"
cp bin64/release/plugins/*.dll "$VANILLA/plugins"
cp bin64/release/themes/*.json "$VANILLA/themes"
cp bin64/release/locales/*.json "$VANILLA/locales"
cp bin64/release/fonts/*.ttf "$VANILLA/fonts"
rm "$VANILLA/musikcore.dll"
pushd $VANILLA
7z a -tzip "musikcube_win64_$VERSION.zip" ./* -mx=9
mv "musikcube_win64_$VERSION.zip" ..
popd