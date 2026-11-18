#!/bin/bash

set -e

rm -rf bin/lib/
rm -f bin/plugins/lib
mkdir -p bin/lib/

PLATFORM=$(uname)

if [[ "$PLATFORM" == 'Darwin' ]]; then
    echo "[stage-vendor-libraries] staging macOS .dylib files..."

    cp vendor/bin/lib/libavcodec-musikcube.62.dylib ./bin/lib
    cp vendor/bin/lib/libavformat-musikcube.62.dylib ./bin/lib
    cp vendor/bin/lib/libavutil-musikcube.60.dylib ./bin/lib
    cp vendor/bin/lib/libswresample-musikcube.6.dylib ./bin/lib
    cp vendor/bin/lib/libcrypto.3.dylib ./bin/lib
    cp vendor/bin/lib/libssl.3.dylib ./bin/lib
    cp vendor/bin/lib/libcurl.4.dylib ./bin/lib
    cp vendor/bin/lib/libmicrohttpd.12.dylib ./bin/lib
    cp vendor/bin/lib/libmp3lame.0.dylib ./bin/lib
    cp vendor/bin/lib/libopenmpt.0.dylib ./bin/lib
    cp vendor/bin/lib/libgme.0.6.4.dylib ./bin/lib
    cp vendor/bin/lib/libtag.2.3.2.dylib ./bin/lib

    mkdir -p ./bin/share/terminfo
    cp -rfp "$(brew --prefix ncurses)"/share/terminfo/* ./bin/share/terminfo

elif [[ "$PLATFORM" == 'Linux' ]]; then
    echo "[stage-vendor-libraries] staging Linux .so files..."

    cp vendor/bin/lib/libavcodec-musikcube.so.62 ./bin/lib
    cp vendor/bin/lib/libavformat-musikcube.so.62 ./bin/lib
    cp vendor/bin/lib/libavutil-musikcube.so.60 ./bin/lib
    cp vendor/bin/lib/libswresample-musikcube.so.6 ./bin/lib
    if [[ -f vendor/bin/lib/libssl.so.3 ]]; then
        cp vendor/bin/lib/lib{crypto,ssl}.so.3 ./bin/lib
    else
        cp vendor/bin/lib/lib{crypto,ssl}.so.1.1 ./bin/lib
    fi
    cp vendor/bin/lib/libcurl.so.4 ./bin/lib
    cp vendor/bin/lib/libmp3lame.so.0 ./bin/lib
    cp vendor/bin/lib/libmicrohttpd.so.12 ./bin/lib
    cp vendor/bin/lib/libopenmpt.so.5 ./bin/lib
    cp vendor/bin/lib/libgme.so.0 ./bin/lib
    cp vendor/bin/lib/libtag.so.2 ./bin/lib

    if [[ $CROSSCOMPILE == rpi-* ]]; then
        XTOOLS_ARCH="armv8-rpi3-linux-gnueabihf"
        if [[ $CROSSCOMPILE == "rpi-armv6" ]]; then
            XTOOLS_ARCH="armv6-rpi-linux-gnueabihf"
        fi
        sysroot="/build/x-tools/${XTOOLS_ARCH}/${XTOOLS_ARCH}/sysroot"
        system_dirs=("$sysroot/usr/lib/arm-linux-gnueabihf" "$sysroot/lib/arm-linux-gnueabihf"
                     "$sysroot/usr/lib" "$sysroot/lib")
    elif [[ $CROSSCOMPILE == "x86" ]]; then
        system_dirs=(/usr/lib/i386-linux-gnu /lib/i386-linux-gnu /usr/lib32 /lib32)
    else
        multiarch=$("${CC:-cc}" -print-multiarch 2>/dev/null || true)
        system_dirs=()
        if [[ -n $multiarch ]]; then
            system_dirs+=("/usr/lib/$multiarch" "/lib/$multiarch")
        fi
        system_dirs+=(/usr/lib64 /lib64 /usr/lib /lib)
    fi

    stage_system_library() {
        local name=$1 dir
        for dir in "${system_dirs[@]}"; do
            if [[ -f "$dir/$name" ]]; then
                cp "$dir/$name" ./bin/lib/
                return
            fi
        done
        if [[ $2 == required ]]; then
            printf 'Missing required runtime library %s (searched %s)\n' "$name" "${system_dirs[*]}" >&2
            return 1
        fi
    }

    for name in libncursesw.so.6 libpanelw.so.6; do
        stage_system_library "$name" required
    done
    # Some distributions fold tinfo into ncurses or link libev statically.
    stage_system_library libtinfo.so.6 optional
    stage_system_library libev.so.4 optional

    mkdir -p ./bin/share/terminfo

    if [[ -d "/lib/terminfo/" ]]; then
        cp -rfp /lib/terminfo/* ./bin/share/terminfo # debian buster
    else
        cp -rfp /usr/share/terminfo/* ./bin/share/terminfo # ubuntu mantic
    fi

fi
