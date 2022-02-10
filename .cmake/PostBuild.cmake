# run `cmake .` again to pick up build plugin build artifacts that we need
# to file glob in. these won't be picked up on the initial build because
# they don't yet exist!
add_custom_target(postbuild ALL DEPENDS musikcube musikcubed)
add_custom_command(TARGET postbuild POST_BUILD COMMAND cmake .)

# strip binaries in release mode
if (CMAKE_BUILD_TYPE MATCHES Release)
  if ((NOT DEFINED DISABLE_STRIP) OR (NOT ${DISABLE_STRIP} MATCHES "true"))
    message(STATUS "[post-build] binary stripping enabled for ${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_command(TARGET postbuild POST_BUILD COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/script/strip-nix.sh" ${CMAKE_CURRENT_SOURCE_DIR})
  else()
    message(STATUS "[post-build] DISABLE_STRIP=true, *NOT* stripping binaries.")
  endif()
endif()

# ensure the binaries can find libmusikcore.so, which lives in the same directory.
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  message(STATUS "[post-build] patching macOS rpath...")
  add_custom_command(TARGET postbuild POST_BUILD COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/script/patch-macos-rpath.sh")
endif()

# copy boost and ffmpeg libraries (which can't be statically linked) to bin/
# note this step also ensures libraries we depend upon have their rpath values
# so libraries are resolved to the local directory.
if (${BUILD_STANDALONE} MATCHES "true")
  message(STATUS "[post-build] copying vendor'd libs...")
  add_custom_command(TARGET postbuild POST_BUILD COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/script/stage-vendor-libraries.sh")
endif()
