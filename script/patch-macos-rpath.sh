#!/bin/sh

install_name_tool -add_rpath "@executable_path/" bin/musikcube
install_name_tool -add_rpath "@executable_path/" bin/musikcubed
exit 0