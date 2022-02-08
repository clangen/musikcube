#!/bin/bash

mkdir -p bin/plugins/
cp vendor/boost-bin/lib/libboost_filesystem.so.1.78.0 ./bin/
cp vendor/boost-bin/lib/libboost_thread.so.1.78.0 ./bin/
cp vendor/ffmpeg-bin/lib/libavcodec-musikcube.so.59 ./bin/plugins/
cp vendor/ffmpeg-bin/lib/libavformat-musikcube.so.59 ./bin/plugins
cp vendor/ffmpeg-bin/lib/libavutil-musikcube.so.57 ./bin/plugins
cp vendor/ffmpeg-bin/lib/libswresample-musikcube.so.4 ./bin/plugins
