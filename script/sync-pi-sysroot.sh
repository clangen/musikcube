#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

HOST=$1
if [[ -z $HOST ]]; then
  printf "usage: sync-pi-sysroot.sh <pi_hostname>"
  exit 1
fi

# https://mechatronicsblog.com/cross-compile-and-deploy-qt-5-12-for-raspberry-pi/
mkdir -p sysroot/usr
mkdir -p sysroot/opt
rsync -avz pi@$HOST:/lib sysroot
rsync -avz pi@$HOST:/usr/include sysroot/usr
rsync -avz pi@$HOST:/usr/lib sysroot/usr
rsync -avz pi@$HOST:/opt/vc sysroot/opt

# https://raw.githubusercontent.com/riscv/riscv-poky/master/scripts/sysroot-relativelinks.py
$SCRIPT_DIR/update-pi-sysroot-symlinks.py ./sysroot
