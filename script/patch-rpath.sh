#!/bin/sh

PLATFORM=$(uname)

if [ "$PLATFORM" = 'Linux' ]; then
    echo "[patch-rpath] patching Linux .so files..."

    chmod -x ./bin/lib/*
    chmod -x ./bin/plugins/*
    chmod -x ./bin/*.so

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

    patchelf --set-rpath "\$ORIGIN:\$ORIGIN/lib" bin/musikcube
    patchelf --set-rpath "\$ORIGIN:\$ORIGIN/lib" bin/musikcubed
fi

if [ "$PLATFORM" = 'Darwin' ]; then
    echo "[patch-rpath] patching macOS binaries..."

    install_name_tool -add_rpath "@executable_path/" bin/musikcube
    install_name_tool -add_rpath "@executable_path/lib" bin/musikcube
    install_name_tool -add_rpath "@executable_path/" bin/musikcubed
    install_name_tool -add_rpath "@executable_path/lib" bin/musikcubed
fi

exit 0
