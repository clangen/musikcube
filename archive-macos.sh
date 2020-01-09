#!/bin/sh

VERSION=$1

if [ -z "$VERSION" ]; then
  echo "usage: ./archive-macos.sh <version>"
  exit
fi

rm -rf bin/

./clean-nix.sh
cmake -DCMAKE_BUILD_TYPE=Release -DLINK_STATICALLY=true -DFFMPEG_ENABLED=false .
make -j4

DIRNAME="musikcube_macos_$VERSION"
OUTPATH="bin/dist/$DIRNAME"

rm -rf "$OUTPATH"

mkdir -p "$OUTPATH/plugins"
mkdir -p "$OUTPATH/locales"
mkdir -p "$OUTPATH/themes"
cp bin/musikcube "$OUTPATH"
cp bin/musikcubed "$OUTPATH"
cp bin/libmusikcore.dylib "$OUTPATH"
cp bin/plugins/*.dylib "$OUTPATH/plugins"
cp bin/locales/*.json "$OUTPATH/locales"
cp bin/themes/*.json "$OUTPATH/themes"

strip bin/musikcube
strip bin/musikcubed
strip bin/libmusikcore.dylib
strip bin/plugins/*.dylib

pushd bin/dist
tar cvf musikcube_macos_$VERSION.tar $DIRNAME
bzip2 musikcube_macos_$VERSION.tar
popd
