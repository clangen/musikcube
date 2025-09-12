#!/bin/bash

rm -rf bin/lib/ 2> /dev/null
rm bin/plugins/lib 2> /dev/null
mkdir -p bin/lib/

PLATFORM=$(uname)

if [[ "$PLATFORM" == 'Darwin' ]]; then
    echo "[stage-vendor-libraries] staging macOS .dylib files..."

    cp vendor/bin/lib/libavcodec-musikcube.62.dylib ./bin/lib
    cp vendor/bin/lib/libavformat-musikcube.62.dylib ./bin/lib
    cp vendor/bin/lib/libavutil-musikcube.60.dylib ./bin/lib
    cp vendor/bin/lib/libswresample-musikcube.6.dylib ./bin/lib
    cp vendor/bin/lib/libopus.0.dylib ./bin/lib
    cp vendor/bin/lib/libogg.0.dylib ./bin/lib
    cp vendor/bin/lib/libvorbis.0.dylib ./bin/lib
    cp vendor/bin/lib/libvorbisenc.2.dylib ./bin/lib
    cp vendor/bin/lib/libcrypto.3.dylib ./bin/lib
    cp vendor/bin/lib/libssl.3.dylib ./bin/lib
    cp vendor/bin/lib/libcurl.4.dylib ./bin/lib
    cp vendor/bin/lib/libmicrohttpd.12.dylib ./bin/lib
    cp vendor/bin/lib/libmp3lame.0.dylib ./bin/lib
    cp vendor/bin/lib/libopenmpt.0.dylib ./bin/lib
    cp vendor/bin/lib/libgme.0.6.3.dylib ./bin/lib
    cp vendor/bin/lib/libtag.2.1.1.dylib ./bin/lib

    mkdir -p ./bin/share/terminfo
    cp -rfp $(brew --prefix)/Cellar/ncurses/6.5/share/terminfo/* ./bin/share/terminfo

elif [[ "$PLATFORM" == 'Linux' ]]; then
    echo "[stage-vendor-libraries] staging Linux .so files..."

    cp vendor/bin/lib/libavcodec-musikcube.so.62 ./bin/lib
    cp vendor/bin/lib/libavformat-musikcube.so.62 ./bin/lib
    cp vendor/bin/lib/libavutil-musikcube.so.60 ./bin/lib
    cp vendor/bin/lib/libswresample-musikcube.so.6 ./bin/lib
    cp vendor/bin/lib/libcrypto.so.3 ./bin/lib 2> /dev/null
    cp vendor/bin/lib/libssl.so.3 ./bin/lib 2> /dev/null
    cp vendor/bin/lib/libcrypto.so.1.1 ./bin/lib 2> /dev/null
    cp vendor/bin/lib/libssl.so.1.1 ./bin/lib 2> /dev/null
    cp vendor/bin/lib/libcurl.so.5 ./bin/lib
    cp vendor/bin/lib/libmp3lame.so.0 ./bin/lib
    cp vendor/bin/lib/libmicrohttpd.so.12 ./bin/lib
    cp vendor/bin/lib/libopenmpt.so.5 ./bin/lib
    cp vendor/bin/lib/libgme.so.0 ./bin/lib
    cp vendor/bin/lib/libtag.so.2 ./bin/lib

    SYSTEM_ROOT=""
    SYSTEM_TYPE="x86_64-linux-gnu"
    if [[ $CROSSCOMPILE == rpi-* ]]; then
        XTOOLS_ARCH="armv8-rpi3-linux-gnueabihf"
        if [[ $CROSSCOMPILE == "rpi-armv6" ]]; then
            XTOOLS_ARCH="armv6-rpi-linux-gnueabihf"
        fi
        SYSTEM_ROOT="/build/x-tools/${XTOOLS_ARCH}/${XTOOLS_ARCH}/sysroot"
        SYSTEM_TYPE="arm-linux-gnueabihf"
    elif [[ $CROSSCOMPILE == "x86" ]]; then
        SYSTEM_TYPE="i386-linux-gnu"
    fi

    cp $SYSTEM_ROOT/lib/$SYSTEM_TYPE/libncursesw.so.6 ./bin/lib
    cp $SYSTEM_ROOT/lib/$SYSTEM_TYPE/libtinfo.so.6 ./bin/lib
    cp $SYSTEM_ROOT/usr/lib/$SYSTEM_TYPE/libpanelw.so.6 ./bin/lib
    cp $SYSTEM_ROOT/usr/lib/$SYSTEM_TYPE/libvorbis.so.0 ./bin/lib
    cp $SYSTEM_ROOT/usr/lib/$SYSTEM_TYPE/libogg.so.0 ./bin/lib
    cp $SYSTEM_ROOT/usr/lib/$SYSTEM_TYPE/libopus.so.0 ./bin/lib
    cp $SYSTEM_ROOT/usr/lib/$SYSTEM_TYPE/libvorbis.so.0 ./bin/lib
    cp $SYSTEM_ROOT/usr/lib/$SYSTEM_TYPE/libvorbisenc.so.2 ./bin/lib

    mkdir -p ./bin/share/terminfo

    if [[ -d "/lib/terminfo/" ]]; then
        cp -rfp /lib/terminfo/* ./bin/share/terminfo # debian buster
    else
        cp -rfp /usr/share/terminfo/* ./bin/share/terminfo # ubuntu mantic
    fi

fi
