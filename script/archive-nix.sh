#!/bin/bash

# set -x

VERSION=$1

if [ -z "$VERSION" ]; then
  echo "usage: archive-nix.sh <version>"
  exit
fi

OS=$(uname)
ARCH=$(uname -m)
OS_ARCH="${OS}-${ARCH}"
SCRIPTDIR=`dirname "$0"`

DLL_EXT="so"
if [ $OS == "Darwin" ]; then
  DLL_EXT="dylib"
fi

OS_SPECIFIC_BUILD_FLAGS=""
if [ $OS == "Linux" ]; then
  OS_SPECIFIC_BUILD_FLAGS="-DENABLE_PIPEWIRE=true"
fi

# ${SCRIPTDIR}/clean-nix.sh
# rm -rf bin/ 2> /dev/null

# cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_PCH=true -DBUILD_STANDALONE=true ${OS_SPECIFIC_BUILD_FLAGS} .
# make -j8

OUTNAME="musikcube_${OS_ARCH}_$VERSION"
OUTDIR="dist/$OUTNAME"

rm -rf $OUTDIR
rm dist/$OUTNAME* 2> /dev/null

mkdir -p $OUTDIR/lib
mkdir -p $OUTDIR/plugins
mkdir -p $OUTDIR/locales
mkdir -p $OUTDIR/themes

cp bin/musikcube $OUTDIR
cp bin/musikcubed $OUTDIR
cp bin/libmusikcore.${DLL_EXT} $OUTDIR
cp bin/lib/* $OUTDIR/lib
cp bin/plugins/*.${DLL_EXT} $OUTDIR/plugins
cp bin/locales/*.json $OUTDIR/locales
cp bin/themes/*.json $OUTDIR/themes

strip $OUTDIR/musikcube
strip $OUTDIR/musikcubed
strip $OUTDIR/libmusikcore.${DLL_EXT}
strip $OUTDIR/lib/*
strip $OUTDIR/libmusikcore.${DLL_EXT}
strip $OUTDIR/plugins/*.${DLL_EXT}

cd dist
tar cvf $OUTNAME.tar $OUTNAME
bzip2 $OUTNAME.tar
cd ..
