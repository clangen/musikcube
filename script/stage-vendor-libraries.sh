#!/bin/bash

set -e

VENDOR_DIR=${1:-vendor}

rm -rf bin/lib/
rm -f bin/plugins/lib
mkdir -p bin/lib/

PLATFORM=$(uname)

if [[ "$PLATFORM" == 'Darwin' ]]; then
    echo "[stage-vendor-libraries] staging macOS .dylib files..."

    cp "$VENDOR_DIR"/bin/lib/libavcodec-musikcube.62.dylib ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libavformat-musikcube.62.dylib ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libavutil-musikcube.60.dylib ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libswresample-musikcube.6.dylib ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libcrypto.3.dylib ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libssl.3.dylib ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libcurl.4.dylib ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libmicrohttpd.12.dylib ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libmp3lame.0.dylib ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libopenmpt.0.dylib ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libgme.0.6.4.dylib ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libtag.2.3.2.dylib ./bin/lib

    mkdir -p ./bin/share/terminfo
    cp -rfp "$(brew --prefix ncurses)"/share/terminfo/* ./bin/share/terminfo

elif [[ "$PLATFORM" == 'Linux' ]]; then
    echo "[stage-vendor-libraries] staging Linux .so files..."

    cp "$VENDOR_DIR"/bin/lib/libavcodec-musikcube.so.62 ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libavformat-musikcube.so.62 ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libavutil-musikcube.so.60 ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libswresample-musikcube.so.6 ./bin/lib
    if [[ -f "$VENDOR_DIR"/bin/lib/libssl.so.3 ]]; then
        cp "$VENDOR_DIR"/bin/lib/lib{crypto,ssl}.so.3 ./bin/lib
    else
        cp "$VENDOR_DIR"/bin/lib/lib{crypto,ssl}.so.1.1 ./bin/lib
    fi
    cp "$VENDOR_DIR"/bin/lib/libcurl.so.4 ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libmp3lame.so.0 ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libmicrohttpd.so.12 ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libopenmpt.so.5 ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libgme.so.0 ./bin/lib
    cp "$VENDOR_DIR"/bin/lib/libtag.so.2 ./bin/lib

    # Search the configured target paths before using platform defaults.
    if [[ -n $MUSIKCUBE_RUNTIME_LIB_DIRS ]]; then
        IFS=: read -r -a system_dirs <<< "$MUSIKCUBE_RUNTIME_LIB_DIRS"
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

    # Stage the target runtime libraries selected by the build profile.
    read -r -a required_libraries <<< "${MUSIKCUBE_REQUIRED_RUNTIME_LIBS-libncursesw.so.6 libpanelw.so.6}"
    # Some distributions fold tinfo into ncurses or link libev statically.
    read -r -a optional_libraries <<< "${MUSIKCUBE_OPTIONAL_RUNTIME_LIBS-libtinfo.so.6 libev.so.4}"
    for name in "${required_libraries[@]}"; do
        stage_system_library "$name" required
    done
    for name in "${optional_libraries[@]}"; do
        stage_system_library "$name" optional
    done

    mkdir -p ./bin/share/terminfo

    terminfo_dir=$MUSIKCUBE_TERMINFO_DIR
    if [[ -z $terminfo_dir ]]; then
        if [[ -d /lib/terminfo ]]; then
            terminfo_dir=/lib/terminfo
        else
            terminfo_dir=/usr/share/terminfo
        fi
    fi
    cp -rfp "$terminfo_dir"/. ./bin/share/terminfo/

fi
