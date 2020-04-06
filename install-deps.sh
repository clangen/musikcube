#!/bin/bash
OS=`uname`
if [ $OS == "Linux" ]; then
  echo "detected linux"
  if [ -x "$(command -v lsb_release)" ]; then
    DISTRO=`lsb_release -cs`
    echo "detected ${DISTRO}"
    case $DISTRO in
      "focal")
        sudo apt-get install build-essential clang cmake libboost-thread1.71-dev libboost-system1.71-dev libboost-filesystem1.71-dev libboost-date-time1.71-dev libboost-atomic1.71-dev libboost-chrono1.71-dev libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncursesw5-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev
        ;;
      "cosmic" | "disco" | "eoan")
        sudo apt-get install build-essential clang cmake libboost-thread1.67-dev libboost-system1.67-dev libboost-filesystem1.67-dev libboost-date-time1.67-dev libboost-atomic1.67-dev libboost-chrono1.67-dev libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncursesw5-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev
        ;;
      "bionic")
        sudo apt-get install build-essential clang cmake libboost-thread1.65-dev libboost-system1.65-dev libboost-filesystem1.65-dev libboost-date-time1.65-dev libboost-atomic1.65-dev libboost-chrono1.65-dev libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncursesw5-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev
        ;;
      "artful")
        sudo apt-get install build-essential clang cmake libboost-thread1.63-dev libboost-system1.63-dev libboost-filesystem1.63-dev libboost-date-time1.63-dev libboost-atomic1.63-dev libboost-chrono1.63-dev libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncursesw5-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev
        ;;
    esac
  fi
  if [ -f "/etc/fedora-release" ]; then
    DISTRO=`cat /etc/fedora-release`
    echo "detected ${DISTRO}"
    sudo dnf install gcc-c++ make cmake boost-devel libogg-devel libvorbis-devel ffmpeg-devel ncurses-devel zlib-devel alsa-lib-devel pulseaudio-libs-devel libcurl-devel libmicrohttpd-devel lame-devel libev-devel taglib-devel libsystemd-devel
  fi
  if [ -f "/etc/arch-release" ] || [ -f "/etc/manjaro-release" ]; then
    sudo pacman -S libogg libvorbis libmicrohttpd ffmpeg lame cmake ncurses boost pulseaudio libpulse alsa-lib curl libev taglib
  fi
fi
if [ $OS == "Darwin" ]; then
  echo "detected macos"
  brew install cmake boost libogg libvorbis ffmpeg libmicrohttpd lame libev taglib
fi
if [ $OS == "FreeBSD" ]; then
  echo "detected freebsd"
  pkg install boost-all curl libvorbis libogg libmicrohttpd ffmpeg alsa-lib cmake sndio libev taglib bash
  portsnap fetch
  portsnap extract (if first time using ports)
  cd /usr/ports/audio/lame
  make reinstall
fi
if [ $OS == "OpenBSD" ]; then
  echo "detected openbsd"
  pkg_add git cmake ffmpeg boost sndio libev libmicrohttpd taglib
fi
