#!/bin/bash

mkdir -p bin/lib/
mkdir -p bin/plugins/
pushd bin/plugins
rm lib 2> /dev/null
ln -s ../lib/
popd

PLATFORM=$(uname)
if [[ "$PLATFORM" == 'Darwin' ]]; then
    echo "[stage-static-vendor-libraries] staging macOS .dylib files..."

    cp vendor/ffmpeg-bin/lib/libavcodec-musikcube.59.dylib ./bin/lib
    cp vendor/ffmpeg-bin/lib/libavformat-musikcube.59.dylib ./bin/lib
    cp vendor/ffmpeg-bin/lib/libavutil-musikcube.57.dylib ./bin/lib
    cp vendor/ffmpeg-bin/lib/libswresample-musikcube.4.dylib ./bin/lib
    cp vendor/ffmpeg-bin/lib/libopus.0.dylib ./bin/lib

    cp vendor/boost-bin/lib/libboost_atomic.dylib ./bin/lib
    cp vendor/boost-bin/lib/libboost_chrono.dylib ./bin/lib
    cp vendor/boost-bin/lib/libboost_date_time.dylib ./bin/lib
    cp vendor/boost-bin/lib/libboost_filesystem.dylib ./bin/lib
    cp vendor/boost-bin/lib/libboost_system.dylib ./bin/lib
    cp vendor/boost-bin/lib/libboost_thread.dylib ./bin/lib

    cp vendor/openssl-bin/lib/libcrypto.dylib ./bin/lib
    cp vendor/openssl-bin/lib/libssl.dylib ./bin/lib

    cp vendor/curl-bin/lib/libcurl.dylib ./bin/lib

    cp vendor/libmicrohttpd-bin/lib/libmicrohttpd.dylib ./bin/lib

elif [[ "$PLATFORM" == 'Linux' ]]; then
    echo "[stage-static-vendor-libraries] staging Linux .so files..."

    cp vendor/ffmpeg-bin/lib/libavcodec-musikcube.so.59 ./bin/lib
    cp vendor/ffmpeg-bin/lib/libavformat-musikcube.so.59 ./bin/lib
    cp vendor/ffmpeg-bin/lib/libavutil-musikcube.so.57 ./bin/lib
    cp vendor/ffmpeg-bin/lib/libswresample-musikcube.so.4 ./bin/lib

    cp vendor/boost-bin/lib/libboost_filesystem.so.1.76.0 ./bin/lib
    cp vendor/boost-bin/lib/libboost_thread.so.1.76.0 ./bin/lib

    cp vendor/openssl-bin/lib/libcrypto.so.1.1 ./bin/lib
    cp vendor/openssl-bin/lib/libssl.so.1.1 ./bin/lib

    cp vendor/curl-bin/lib/libcurl.so.4 ./bin/lib

    cp vendor/lame-bin/lib/libmp3lame.so.0 ./bin/lib

    cp vendor/libmicrohttpd-bin/lib/libmicrohttpd.so.12 ./bin/lib

    cp /lib/x86_64-linux-gnu/libz.so.1 ./bin/lib
    cp /lib/x86_64-linux-gnu/libopenmpt.so.0 ./bin/lib
    cp /lib/x86_64-linux-gnu/libmpg123.so.0 ./bin/lib
    cp /lib/x86_64-linux-gnu/libvorbis.so.0 ./bin/lib
    cp /lib/x86_64-linux-gnu/libvorbisfile.so.3 ./bin/lib
    cp /lib/x86_64-linux-gnu/libogg.so.0 ./bin/lib
    cp /lib/x86_64-linux-gnu/libncursesw.so.6 ./bin/lib
    cp /lib/x86_64-linux-gnu/libpanelw.so.6 ./bin/lib
    cp /lib/x86_64-linux-gnu/libtinfo.so.6 ./bin/lib
    cp /lib/x86_64-linux-gnu/libopus.so.0 ./bin/lib

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
