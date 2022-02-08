#!/bin/sh

install_name_tool -add_rpath "@executable_path/" bin/musikcube
install_name_tool -add_rpath "@executable_path/lib" bin/musikcube
install_name_tool -add_rpath "@executable_path/" bin/musikcubed
install_name_tool -add_rpath "@executable_path/lib" bin/musikcubed
exit 0