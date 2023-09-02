#!/bin/bash

rm -rf bin/lib/ 2> /dev/null
rm bin/plugins/lib 2> /dev/null
mkdir -p bin/lib/

PLATFORM=$(uname)

if [[ "$PLATFORM" == 'Darwin' ]]; then
    echo "[stage-vendor-libraries] staging macOS .dylib files..."

    cp vendor/bin/lib/libavcodec-musikcube.60.dylib ./bin/lib
    cp vendor/bin/lib/libavformat-musikcube.60.dylib ./bin/lib
    cp vendor/bin/lib/libavutil-musikcube.58.dylib ./bin/lib
    cp vendor/bin/lib/libswresample-musikcube.4.dylib ./bin/lib
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
    cp vendor/bin/lib/libtag.1.19.0.dylib ./bin/lib

    mkdir -p ./bin/share/terminfo
    cp -rfp $(brew --prefix)/Cellar/ncurses/6.4/share/terminfo/* ./bin/share/terminfo

elif [[ "$PLATFORM" == 'Linux' ]]; then
    echo "[stage-vendor-libraries] staging Linux .so files..."

    cp vendor/bin/lib/libavcodec-musikcube.so.60 ./bin/lib
    cp vendor/bin/lib/libavformat-musikcube.so.60 ./bin/lib
    cp vendor/bin/lib/libavutil-musikcube.so.58 ./bin/lib
    cp vendor/bin/lib/libswresample-musikcube.so.4 ./bin/lib
    cp vendor/bin/lib/libcrypto.so.3 ./bin/lib 2> /dev/null
    cp vendor/bin/lib/libssl.so.3 ./bin/lib 2> /dev/null
    cp vendor/bin/lib/libcrypto.so.1.1 ./bin/lib 2> /dev/null
    cp vendor/bin/lib/libssl.so.1.1 ./bin/lib 2> /dev/null
    cp vendor/bin/lib/libcurl.so.4 ./bin/lib
    cp vendor/bin/lib/libmp3lame.so.0 ./bin/lib
    cp vendor/bin/lib/libmicrohttpd.so.12 ./bin/lib
    cp vendor/bin/lib/libopenmpt.so.0 ./bin/lib
    cp vendor/bin/lib/libgme.so.0 ./bin/lib
    cp vendor/bin/lib/libtag.so.1 ./bin/lib

    SYSTEM_ROOT=""
    SYSTEM_TYPE="x86_64-linux-gnu"
    if [[ $CROSSCOMPILE == rpi-* ]]; then
        SYSTEM_ROOT="/build/${CROSSCOMPILE}/sysroot"
        SYSTEM_TYPE="arm-linux-gnueabihf"
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
    cp -rfp /lib/terminfo/* ./bin/share/terminfo

fi
