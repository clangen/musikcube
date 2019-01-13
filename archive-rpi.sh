#!/bin/sh

./clean-nix.sh
cmake -DGENERATE_DEB=1 -DDEB_ARCHITECTURE=armhf -DDEB_DISTRO=stretch -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .
make -j2
cpack
