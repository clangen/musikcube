#!/bin/bash

rm -rf bin/lib/ 2> /dev/null

mkdir -p bin/lib/
mkdir -p bin/plugins/
pushd bin/plugins
rm lib 2> /dev/null
ln -s ../lib/
popd

PLATFORM=$(uname)

if [[ "$PLATFORM" == 'Darwin' ]]; then
    echo "[stage-static-vendor-libraries] staging macOS .dylib files..."

    cp vendor/bin/lib/libavcodec-musikcube.59.dylib ./bin/lib
    cp vendor/bin/lib/libavformat-musikcube.59.dylib ./bin/lib
    cp vendor/bin/lib/libavutil-musikcube.57.dylib ./bin/lib
    cp vendor/bin/lib/libswresample-musikcube.4.dylib ./bin/lib
    cp vendor/bin/lib/libopus.0.dylib ./bin/lib
    cp vendor/bin/lib/libogg.0.dylib ./bin/lib
    cp vendor/bin/lib/libvorbis.0.dylib ./bin/lib
    cp vendor/bin/lib/libvorbisenc.2.dylib ./bin/lib
    cp vendor/bin/lib/libboost_atomic.dylib ./bin/lib
    cp vendor/bin/lib/libboost_chrono.dylib ./bin/lib
    cp vendor/bin/lib/libboost_date_time.dylib ./bin/lib
    cp vendor/bin/lib/libboost_filesystem.dylib ./bin/lib
    cp vendor/bin/lib/libboost_system.dylib ./bin/lib
    cp vendor/bin/lib/libboost_thread.dylib ./bin/lib
    cp vendor/bin/lib/libcrypto.3.dylib ./bin/lib
    cp vendor/bin/lib/libssl.3.dylib ./bin/lib
    cp vendor/bin/lib/libcurl.4.dylib ./bin/lib
    cp vendor/bin/lib/libmicrohttpd.12.dylib ./bin/lib
    cp vendor/bin/lib/libmp3lame.0.dylib ./bin/lib
    cp vendor/bin/lib/libopenmpt.0.dylib ./bin/lib

    mkdir -p ./bin/share/terminfo
    cp -rfp $(brew --prefix)/Cellar/ncurses/6.3/share/terminfo/* ./bin/share/terminfo

elif [[ "$PLATFORM" == 'Linux' ]]; then
    echo "[stage-static-vendor-libraries] staging Linux .so files..."

    cp vendor/bin/lib/libavcodec-musikcube.so.59 ./bin/lib
    cp vendor/bin/lib/libavformat-musikcube.so.59 ./bin/lib
    cp vendor/bin/lib/libavutil-musikcube.so.57 ./bin/lib
    cp vendor/bin/lib/libswresample-musikcube.so.4 ./bin/lib
    cp vendor/bin/lib/libboost_atomic.so.1.80.0 ./bin/lib
    cp vendor/bin/lib/libboost_chrono.so.1.80.0 ./bin/lib
    cp vendor/bin/lib/libboost_date_time.so.1.80.0 ./bin/lib
    cp vendor/bin/lib/libboost_filesystem.so.1.80.0 ./bin/lib
    cp vendor/bin/lib/libboost_system.so.1.80.0 ./bin/lib
    cp vendor/bin/lib/libboost_thread.so.1.80.0 ./bin/lib
    cp vendor/bin/lib/libcrypto.so.3 ./bin/lib
    cp vendor/bin/lib/libssl.so.3 ./bin/lib
    cp vendor/bin/lib/libcurl.so.4 ./bin/lib
    cp vendor/bin/lib/libmp3lame.so.0 ./bin/lib
    cp vendor/bin/lib/libmicrohttpd.so.12 ./bin/lib
    cp vendor/bin/lib/libopenmpt.so.0 ./bin/lib

    SYSTEM_ROOT=""
    SYSTEM_TYPE="x86_64-linux-gnu"
    if [[ $CROSSCOMPILE == "rpi" ]]; then
        SYSTEM_ROOT="/build/rpi/sysroot"
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

    # update the RPATH so libraries in libs/ can discover each other,
    # and plugins can discover themselves, and libs/ (but not the
    # other way around)

    FILES="./bin/lib/*"
    for f in $FILES
    do
        patchelf --set-rpath "\$ORIGIN" "$f"
    done

    FILES="./bin/plugins/*.so"
    for f in $FILES
    do
        patchelf --set-rpath "\$ORIGIN:\$ORIGIN/../lib" "$f"
    done

    chmod -x ./bin/lib/*
fi
