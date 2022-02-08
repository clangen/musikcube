# ensure the binaries can find libmusikcore.so, which lives in the same directory.
if (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
  # in Linux/BSD distributions we can specify the magic "$ORIGIN" value for
  # the rpath for the dymamic linker to search the executable's directory
  set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/lib/")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  # note: macOS does not use $ORIGIN, and uses @executable_path/ instead. we
  # add a custom post build step at the bottom of this file to add this value
  # to macOS binaries.
endif()