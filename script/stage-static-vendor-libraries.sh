#!/bin/bash

PLATFORM=$(uname)
if [[ "$PLATFORM" == 'Darwin' ]]; then
    echo "[stage-static-vendor-libraries] no-op on darwin. not required."
    exit 0
fi

mkdir -p bin/lib/
mkdir -p bin/plugins/

cp vendor/boost-bin/lib/libboost_filesystem.so.1.78.0 ./bin/lib/
cp vendor/boost-bin/lib/libboost_thread.so.1.78.0 ./bin/lib/
cp vendor/ffmpeg-bin/lib/libavcodec-musikcube.so.59 ./bin/lib/
cp vendor/ffmpeg-bin/lib/libavformat-musikcube.so.59 ./bin/lib/
cp vendor/ffmpeg-bin/lib/libavutil-musikcube.so.57 ./bin/lib/
cp vendor/ffmpeg-bin/lib/libswresample-musikcube.so.4 ./bin/lib/
cp vendor/curl-bin/lib/libcurl.so.4 ./bin/lib/

cp /lib/x86_64-linux-gnu/libssl.so.1.1 ./bin/lib/
cp /lib/x86_64-linux-gnu/libcrypto.so.1.1 ./bin/lib/
cp /lib/x86_64-linux-gnu/libz.so.1 ./bin/lib/
cp /lib/x86_64-linux-gnu/libmp3lame.so.0 ./bin/lib/
cp /lib/x86_64-linux-gnu/libopenmpt.so.0 ./bin/lib
cp /lib/x86_64-linux-gnu/libmpg123.so.0 ./bin/lib
cp /lib/x86_64-linux-gnu/libvorbis.so.0 ./bin/lib
cp /lib/x86_64-linux-gnu/libvorbisfile.so.3 ./bin/lib
cp /lib/x86_64-linux-gnu/libogg.so.0 ./bin/lib
