#!/bin/bash

# set -x

VERSION=$1

if [[ -z "$VERSION" ]]; then
  echo "usage: archive-nix.sh <version>"
  exit
fi

OS=$(uname)

JOBS="-j8"

FRIENDLY_OS_NAME="linux"
if [[ $OS == "Darwin" ]]; then
  FRIENDLY_OS_NAME="macos"
  JOBS="-j$(sysctl -n hw.ncpu)"
fi

ARCH=$(uname -m)
DEB_ARCH=$ARCH
VENDOR=$ARCH
if [[ -n $CROSSCOMPILE ]]; then
  FRIENDLY_OS_NAME="linux_${CROSSCOMPILE}"
  VENDOR=${CROSSCOMPILE}
  ARCH="armhf"
  DEB_ARCH="armhf"
elif [[ $ARCH == "x86_64" ]]; then
  DEB_ARCH="amd64"
fi

OS_ARCH="${FRIENDLY_OS_NAME}_${ARCH}"
OUTNAME="musikcube_${OS_ARCH}_$VERSION"
OUTDIR="dist/$VERSION/$OUTNAME"
SCRIPTDIR=`dirname "$0"`
CMAKE_TOOLCHAIN=""

DLL_EXT="so"
if [[ $OS == "Darwin" ]]; then
  DLL_EXT="dylib"
fi

OS_SPECIFIC_BUILD_FLAGS=""
if [[ $OS == "Linux" ]]; then
  OS_SPECIFIC_BUILD_FLAGS="-DGENERATE_DEB=true -DPACKAGE_ARCHITECTURE=${DEB_ARCH} -DCMAKE_INSTALL_PREFIX=/usr"
  if [[ $CROSSCOMPILE == "rpi-armv7a" ]]; then
    # for now we don't support pipewire when cross compiling...
    OS_SPECIFIC_BUILD_FLAGS="$OS_SPECIFIC_BUILD_FLAGS -DENABLE_PIPEWIRE=false"
  fi
fi

if [[ $CROSSCOMPILE == "rpi-armv7a" ]]; then
  CMAKE_TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=.cmake/RaspberryPiToolchain.cmake"
fi

rm vendor
ln -s ../vendor-$VENDOR/ ./vendor
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
  cmake ${CMAKE_TOOLCHAIN} -DCMAKE_BUILD_TYPE=Release -DBUILD_STANDALONE=true -DENABLE_PCH=true ${OS_SPECIFIC_BUILD_FLAGS} . || exit $?
  make ${JOBS} || exit $?
fi

./script/patch-rpath.sh $(pwd) || exit $?

rm -rf dist/$VERSION/*${OS_ARCH}_$VERSION* 2> /dev/null

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

if [[ $CROSSCOMPILE == "rpi-armv7a" ]]; then
  printf "\n\n\n     ***** CROSSCOMPILE DETECTED, **NOT** SCANNING DEPENDENCIES! *****\n\n\n"
  sleep 1
else
  printf "\n\n\n     ***** SCANNING DEPENDENCIES *****\n\n\n"
  sleep 1
  node ./script/scan-standalone dist/$VERSION/$OUTNAME || exit $?
fi

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

if [[ $OS == "Linux" ]]; then
  # hack so the pre-install script doesn't re-run, clobbering binaries that
  # have had their rpaths updated and symbols stripped.
  # https://stackoverflow.com/a/57531164
  perl -i.bak -0pe "s|Unix Makefiles|Ninja|" CPackConfig.cmake
  cpack
  mv *.deb dist/$VERSION/
  mv *.rpm dist/$VERSION/
fi

printf "\n\n\n     ***** DONE *****\n\n\n"
ls -al dist/$VERSION
printf "\n\n"
