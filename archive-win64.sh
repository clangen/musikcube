#!/bin/sh

VERSION=$1

if [ -z "$VERSION" ]; then
  echo "usage: ./archive-win64.sh <version>"
  exit
fi

VANILLA="bin64/dist/musikcube_win64_$VERSION"

rm -rf "$VANILLA"

mkdir -p "$VANILLA/plugins"
mkdir -p "$VANILLA/themes"
mkdir -p "$VANILLA/locales"
mkdir -p "$VANILLA/fonts"
cp bin64/release/musikcube.exe "$VANILLA" 
cp bin64/release/*.dll "$VANILLA" 
cp bin64/release/plugins/*.dll "$VANILLA/plugins"
cp bin64/release/themes/*.json "$VANILLA/themes"
cp bin64/release/locales/*.json "$VANILLA/locales"
cp bin64/release/fonts/*.ttf "$VANILLA/fonts"
pushd $VANILLA
7z a -tzip "musikcube_win64_$VERSION.zip" ./* -mx=9
mv "musikcube_win64_$VERSION.zip" ..
popd
