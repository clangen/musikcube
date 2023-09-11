#!/bin/bash

# set -x

VERSION=$1

if [[ -z "$VERSION" ]]; then
  echo "usage: archive-nix.sh <version>"
  exit
fi

OS=$(uname)

JOBS="-j8"
if [[ $OS == "Darwin" ]]; then
    JOBS="-j$(sysctl -n hw.ncpu)"
elif nproc &> /dev/null; then
    JOBS="-j$(nproc)"
fi

FRIENDLY_OS_NAME="linux"
if [[ $OS == "Darwin" ]]; then
  FRIENDLY_OS_NAME="macos"
  JOBS="-j$(sysctl -n hw.ncpu)"
fi

STRIP="strip"
FRIENDLY_ARCH_NAME=$(uname -m)
DEB_ARCH=$FRIENDLY_ARCH_NAME
VENDOR=$FRIENDLY_ARCH_NAME
if [[ $CROSSCOMPILE == rpi-* ]]; then
  FRIENDLY_OS_NAME="linux_rpi"
  XTOOLS_NAME="armv8-rpi3-linux-gnueabihf"
  VENDOR=${CROSSCOMPILE}
  FRIENDLY_ARCH_NAME="armv8"
  DEB_ARCH="armhf"
  if [[ $CROSSCOMPILE == "rpi-armv6" ]]; then
    XTOOLS_NAME="armv6-rpi-linux-gnueabihf"
    FRIENDLY_ARCH_NAME="armv6"
  fi
  STRIP="/build/x-tools/${XTOOLS_NAME}/${XTOOLS_NAME}/bin/strip"
elif [[ $CROSSCOMPILE == "x86" ]]; then
  VENDOR=${CROSSCOMPILE}
  FRIENDLY_ARCH_NAME="x86"
  DEB_ARCH="x86"
elif [[ $FRIENDLY_ARCH_NAME == "x86_64" ]]; then
  DEB_ARCH="amd64"
fi

OS_ARCH="${FRIENDLY_OS_NAME}_${FRIENDLY_ARCH_NAME}"
OUTNAME="musikcube_$VERSION_${OS_ARCH}"
OUTDIR="dist/$VERSION/$OUTNAME"
SCRIPTDIR=`dirname "$0"`
CMAKE_TOOLCHAIN=""

DLL_EXT="so"
if [[ $OS == "Darwin" ]]; then
  DLL_EXT="dylib"
fi

OS_SPECIFIC_BUILD_FLAGS=""
if [[ $OS == "Linux" ]]; then
  OS_SPECIFIC_BUILD_FLAGS="-DGENERATE_DEB=true -DPACKAGE_ARCHITECTURE=${DEB_ARCH} -DFRIENDLY_ARCHITECTURE_NAME=${OS_ARCH} -DCMAKE_INSTALL_PREFIX=/usr"
  if [[ $CROSSCOMPILE == rpi-* ]]; then
    # for now we don't support pipewire when cross compiling...
    OS_SPECIFIC_BUILD_FLAGS="$OS_SPECIFIC_BUILD_FLAGS -DENABLE_PIPEWIRE=false"
  fi
fi

if [[ $CROSSCOMPILE == "rpi-armv8" ]]; then
  CMAKE_TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=.cmake/RaspberryPiToolchain-armv8.cmake"
elif [[ $CROSSCOMPILE == "rpi-armv6" ]]; then
  CMAKE_TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=.cmake/RaspberryPiToolchain-armv6.cmake"
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
  cmake ${CMAKE_TOOLCHAIN} -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_STANDALONE=true -DENABLE_PCH=true ${OS_SPECIFIC_BUILD_FLAGS} . || exit $?
  make ${JOBS} || exit $?
fi

./script/patch-rpath.sh $(pwd) || exit $?

printf "stripping binaries..."
$STRIP bin/musikcube
$STRIP bin/musikcubed
$STRIP bin/libmusikcore.${DLL_EXT}
$STRIP bin/lib/*
$STRIP bin/libmusikcore.${DLL_EXT}
$STRIP bin/plugins/*.${DLL_EXT}

printf "staging binaries..."
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

if [[ $CROSSCOMPILE == rpi-* ]]; then
  printf "\n\n\n     ***** RPI CROSSCOMPILE DETECTED, **NOT** SCANNING DEPENDENCIES! *****\n\n\n"
  sleep 1
else
  printf "\n\n\n     ***** SCANNING DEPENDENCIES *****\n\n\n"
  sleep 1
  node ./script/scan-standalone dist/$VERSION/$OUTNAME || exit $?
fi

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
