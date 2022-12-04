#!/bin/bash

PLATFORM=$(uname)

if [[ "$PLATFORM" == 'Darwin' ]]; then
    echo "[patch-macos-rpath] patch macOS binaries..."

    install_name_tool -add_rpath "@executable_path/" bin/musikcube
    install_name_tool -add_rpath "@executable_path/lib" bin/musikcube
    install_name_tool -add_rpath "@executable_path/" bin/musikcubed
    install_name_tool -add_rpath "@executable_path/lib" bin/musikcubed
fi

exit 0