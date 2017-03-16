#!/bin/sh

VERSION=$1

if [ -z "$VERSION" ]; then
  echo "usage: ./archive-macos.sh <version>"
  exit
fi

rm bin/musikbox
rm -rf bin/musikbox/plugins

cmake -DCMAKE_BUILD_TYPE=Release -DLINK_STATICALLY=true .
make -j4

DIRNAME="musikbox_macos_$VERSION"
OUTPATH="bin/dist/$DIRNAME"

rm -rf "$OUTPATH"

mkdir -p "$OUTPATH/plugins"
mkdir -p "$OUTPATH/locales"
mkdir -p "$OUTPATH/themes"
cp bin/musikbox "$OUTPATH" 
cp bin/plugins/*.dylib "$OUTPATH/plugins"
cp bin/locales/*.json "$OUTPATH/locales"
cp bin/themes/*.json "$OUTPATH/themes"

pushd bin/dist 
tar cvf musikbox_macos_static_$VERSION.tar $DIRNAME
bzip2 musikbox_macos_static_$VERSION.tar
popd
