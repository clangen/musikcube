#!/bin/bash
OS=`uname`
if [ $OS == "Linux" ]; then
  echo "detected linux"
  if [ -x "$(command -v lsb_release)" ]; then
    DISTRO=`lsb_release -cs`
    echo "detected ${DISTRO}"
    case $DISTRO in
      "impish")
        sudo apt-get install build-essential clang cmake libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncurses-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev libavcodec-extra libopenmpt-dev libssl-dev libspa-0.2-dev libpipewire-0.3-dev pipewire pipewire-bin pipewire-audio-client-libraries
	      ;;
      "hirsute")
        sudo apt-get install build-essential clang cmake libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncurses-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev libavcodec-extra libopenmpt-dev libssl-dev libspa-0.2-dev libpipewire-0.3-dev pipewire pipewire-bin pipewire-audio-client-libraries
	      ;;
      "focal" | "groovy" | "ulyssa")
        sudo apt-get install build-essential clang cmake libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncurses-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev libavcodec-extra libopenmpt-dev libssl-dev
        ;;
      "buster")
        sudo apt-get install build-essential clang cmake libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncursesw5-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev libavcodec-extra libopenmpt-dev libssl-dev
        ;;
      "bullseye")
        sudo apt-get install build-essential clang cmake libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncursesw5-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev libavcodec-extra58 libopenmpt-dev libssl-dev
        ;;
      "cosmic" | "disco" | "eoan")
        sudo apt-get install build-essential clang cmake libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncursesw5-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev libavcodec-extra libopenmpt-dev libssl-dev
        ;;
      "bionic")
        sudo apt-get install build-essential clang cmake libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncursesw5-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev libavcodec-extra libopenmpt-dev libssl-dev
        ;;
      "artful")
        sudo apt-get install build-essential clang cmake libogg-dev libvorbis-dev libavutil-dev libavformat-dev libswresample-dev libncursesw5-dev libasound2-dev libpulse-dev pulseaudio libmicrohttpd-dev libmp3lame-dev libcurl4-openssl-dev libev-dev libtag1-dev libsystemd-dev libavcodec-extra libopenmpt-dev libssl-dev
        ;;
    esac
  fi
  if [ -f "/etc/fedora-release" ]; then
    DISTRO=`cat /etc/fedora-release`
    VERSION=$(cat /etc/fedora-release | awk '{ print $3 }')
    echo "detected ${DISTRO} (version=${VERSION})"
    sudo dnf install -y https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm rpm-build rpmdevtools 'dnf-command(builddep)'
    sudo dnf install gcc-c++ make cmake libogg-devel libvorbis-devel ffmpeg-devel ncurses-devel zlib-devel alsa-lib-devel pulseaudio-libs-devel libcurl-devel libmicrohttpd-devel lame-devel libev-devel openssl-devel taglib-devel systemd-devel libopenmpt-devel
    case $VERSION in
      "34" | "35")
      sudo dnf install pipewire-devel pipewire-utils pipewire-libs
      ;;
    esac
  fi
  if [ -f "/etc/arch-release" ] || [ -f "/etc/manjaro-release" ]; then
    sudo pacman -S libogg libvorbis libmicrohttpd ffmpeg lame cmake ncurses pulseaudio libpulse alsa-lib curl libev taglib libopenmpt
  fi
  if [ -f "/etc/SUSE-brand" ]; then
    echo "detected SUSE"
    sudo zypper install libcurl-devel libmicrohttpd-devel cmake ncurses-devel libogg-devel libvorbis-devel ffmpeg-3-libavcodec-devel ffmpeg-3-libswresample-devel ffmpeg-3-libavformat-devel ffmpeg-3-libavutil-devel libmp3lame-devel pulseaudio libpulse-devel alsa-devel zlib-devel libressl-devel libev-devel libtag-devel systemd-devel libopenmpt-devel
  fi
fi
if [ $OS == "Darwin" ]; then
  echo "detected macos"
  brew install cmake libogg libvorbis ffmpeg libmicrohttpd lame libev taglib libopenmpt
fi
if [ $OS == "FreeBSD" ]; then
  echo "detected freebsd"
  pkg install ncurses curl libvorbis libogg libmicrohttpd ffmpeg alsa-lib cmake sndio libev taglib bash libopenmpt
  portsnap fetch
  portsnap extract
  cd /usr/ports/audio/lame
  make reinstall
fi
if [ $OS == "OpenBSD" ]; then
  echo "detected openbsd"
  pkg_add git cmake ffmpeg sndio libev libmicrohttpd taglib libopenmpt
fi
