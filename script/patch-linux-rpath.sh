#!/bin/bash

PLATFORM=$(uname)

if [[ "$PLATFORM" == 'Linux' ]]; then
    echo "[patch-linux-rpath] patch Linux .so files..."

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
