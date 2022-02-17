#!/bin/bash

# set -x

VERSION=$1

if [ -z "$VERSION" ]; then
  echo "usage: archive-nix.sh <version>"
  exit
fi

OS=$(uname)

JOBS="-j8"
FRIENDLY_OS_NAME="linux"
if [ $OS == "Darwin" ]; then
  FRIENDLY_OS_NAME="macos"
  JOBS="-j$(sysctl -n hw.ncpu)"
fi

ARCH=$(uname -m)
OS_ARCH="${FRIENDLY_OS_NAME}_${ARCH}"
OUTNAME="musikcube_standalone_${OS_ARCH}_$VERSION"
OUTDIR="dist/$VERSION/$OUTNAME"
SCRIPTDIR=`dirname "$0"`
CMAKE_TOOLCHAIN=""

DLL_EXT="so"
if [ $OS == "Darwin" ]; then
  DLL_EXT="dylib"
fi

OS_SPECIFIC_BUILD_FLAGS=""
if [ $OS == "Linux" ]; then
  OS_SPECIFIC_BUILD_FLAGS="-DENABLE_PIPEWIRE=true -DGENERATE_DEB=true -DDEB_ARCHITECTURE=amd64 -DCMAKE_INSTALL_PREFIX=/usr"
fi

rm vendor
if [[ $CROSSCOMPILE == "rpi" ]]; then
  ln -s ../vendor-arm/ ./vendor
  CMAKE_TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=.cmake/RaspberryPiToolchain.cmake"
else
  ln -s ../vendor-$ARCH/ ./vendor
fi

printf "\nsetup symlink:\n"
ls -ald vendor

printf "\n"
read -p 'clean and rebuild [y]? ' CLEAN
if [[ $CLEAN == 'n' || $CLEAN == 'N' ]]; then
  printf "\n\n\n     ***** SKIPPING REBUILD *****\n\n\n"
  ./script/stage-vendor-libraries.sh || exit $?
  sleep 3
else
  printf "\n\n\n     ***** REBUILDING NOW *****\n\n\n"
  sleep 3
  ${SCRIPTDIR}/clean-nix.sh
  rm -rf bin/ 2> /dev/null
  ./script/stage-vendor-libraries.sh || exit $?
  cmake ${CMAKE_TOOLCHAIN} -DCMAKE_BUILD_TYPE=Release -DBUILD_STANDALONE=true ${OS_SPECIFIC_BUILD_FLAGS} . || exit $?
  make ${JOBS} || exit $?
fi

rm -rf dist/$VERSION 2> /dev/null

mkdir -p $OUTDIR/lib
mkdir -p $OUTDIR/plugins
mkdir -p $OUTDIR/locales
mkdir -p $OUTDIR/themes
mkdir -p $OUTDIR/share/terminfo

cp bin/musikcube $OUTDIR
cp bin/musikcubed $OUTDIR
cp bin/libmusikcore.${DLL_EXT} $OUTDIR
cp bin/lib/* $OUTDIR/lib
cp bin/plugins/*.${DLL_EXT} $OUTDIR/plugins
cp bin/locales/*.json $OUTDIR/locales
cp bin/themes/*.json $OUTDIR/themes
cp -rfp bin/share/terminfo/* $OUTDIR/share/terminfo/

strip $OUTDIR/musikcube
strip $OUTDIR/musikcubed
strip $OUTDIR/libmusikcore.${DLL_EXT}
strip $OUTDIR/lib/*
strip $OUTDIR/libmusikcore.${DLL_EXT}
strip $OUTDIR/plugins/*.${DLL_EXT}

cd dist/$VERSION/
tar cvf $OUTNAME.tar $OUTNAME
bzip2 $OUTNAME.tar
cd ../../

if [ $OS == "Linux" ]; then
  cpack
  mv *.deb dist/$VERSION/
  mv *.rpm dist/$VERSION/
fi

printf "\n\n\n     ***** DONE *****\n\n\n"
ls -al dist/$VERSION
printf "\n\n"
