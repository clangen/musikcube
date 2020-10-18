#!/bin/sh

SCRIPT_DIR=`dirname "$0"`

rm -rf bin
${SCRIPT_DIR}/clean-nix.sh
cmake -DGENERATE_DEB=1 -DDEB_ARCHITECTURE=armhf -DDEB_PLATFORM=rpi -DDEB_DISTRO=buster -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .
make -j4
cpack
