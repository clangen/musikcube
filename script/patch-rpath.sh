#!/bin/sh

set -eu

PLATFORM=$(uname)

if [ "$PLATFORM" = 'Linux' ]; then
    echo "[patch-rpath] patching Linux .so files..."

    for f in ./bin/lib/* ./bin/plugins/*.so ./bin/*.so; do
        if [ -f "$f" ]; then
            chmod -x "$f"
        fi
    done

    # plugins also need the core library in the parent directory.

    FILES="./bin/lib/*"
    for f in $FILES
    do
        [ -f "$f" ] || continue
        patchelf --set-rpath "\$ORIGIN" "$f" || exit $?
    done

    FILES="./bin/plugins/*.so"
    for f in $FILES
    do
        patchelf --set-rpath "\$ORIGIN:\$ORIGIN/..:\$ORIGIN/../lib" "$f" || exit $?
    done

    patchelf --set-rpath "\$ORIGIN:\$ORIGIN/lib" bin/musikcube || exit $?
    patchelf --set-rpath "\$ORIGIN:\$ORIGIN/lib" bin/musikcubed || exit $?
    patchelf --set-rpath "\$ORIGIN:\$ORIGIN/lib" bin/libmusikcore.so || exit $?
fi

if [ "$PLATFORM" = 'Darwin' ]; then
    echo "[patch-rpath] patching macOS binaries..."
    SOURCE_DIR=$1

    add_rpath() {
        binary=$1
        rpath=$2
        if ! otool -l "$binary" | awk -v expected="$rpath" '
            $1 == "cmd" && $2 == "LC_RPATH" { in_rpath = 1; next }
            in_rpath && $1 == "path" {
                if ($2 == expected) found = 1
                in_rpath = 0
            }
            END { exit !found }
        '; then
            install_name_tool -add_rpath "$rpath" "$binary"
        fi
    }

    for binary in "$SOURCE_DIR/bin/musikcube" "$SOURCE_DIR/bin/musikcubed"; do
        add_rpath "$binary" "@executable_path/"
        add_rpath "$binary" "@executable_path/lib"
    done
fi

exit 0
