# plugin dynamic libraries
if (APPLE)
  set(plugin_extension dylib)
else ()
  set(plugin_extension so)
endif ()

if (${BUILD_STANDALONE} MATCHES "true")
  install(DIRECTORY "${MUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY}/lib/"
    DESTINATION share/musikcube/lib COMPONENT MusikcoreSDK OPTIONAL)
  install(DIRECTORY "${MUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY}/share/"
    DESTINATION share/musikcube/share OPTIONAL)
endif()

install(DIRECTORY "${MUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY}/plugins/"
  DESTINATION share/musikcube/plugins OPTIONAL FILES_MATCHING PATTERN "*.${plugin_extension}")

# sdk header files
file(GLOB sdk_headers "src/musikcore/sdk/*.h")
install(FILES ${sdk_headers} DESTINATION include/musikcube/musikcore/sdk)

# resources
file(GLOB themes "src/musikcube/data/themes/*.json")
file(COPY ${themes} DESTINATION bin/themes)
install(FILES ${themes} DESTINATION share/musikcube/themes)

file(GLOB locales "src/musikcube/data/locales/*.json")
file(COPY ${locales} DESTINATION bin/locales)
install(FILES ${locales} DESTINATION share/musikcube/locales)

# linux desktop shortcuts
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  file(GLOB linux_share_applications "src/musikcube/data/linux/share/applications/musikcube.desktop")
  install(FILES ${linux_share_applications} DESTINATION share/applications/)
  file(GLOB linux_share_icons_48 "src/musikcube/data/linux/share/icons/hicolor/48x48/apps/*")
  install(FILES ${linux_share_icons_48} DESTINATION share/icons/hicolor/48x48/apps/)
  file(GLOB linux_share_icons_64 "src/musikcube/data/linux/share/icons/hicolor/64x64/apps/*")
  install(FILES ${linux_share_icons_64} DESTINATION share/icons/hicolor/64x64/apps/)
  file(GLOB linux_share_icons_128 "src/musikcube/data/linux/share/icons/hicolor/128x128/apps/*")
  install(FILES ${linux_share_icons_128} DESTINATION share/icons/hicolor/128x128/apps/)
endif()

# Application SDK uses the same shared runtime as the installed frontend.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(MUSIKCORE_INSTALL_RUNTIME_DIR share/musikcube)
set(MUSIKCORE_LIBRARY_FILENAME "${CMAKE_SHARED_LIBRARY_PREFIX}musikcore${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(musikcore_config_dir "${CMAKE_INSTALL_LIBDIR}/cmake/musikcore")
configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/musikcoreConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/musikcoreConfig.cmake"
  INSTALL_DESTINATION "${musikcore_config_dir}"
  PATH_VARS CMAKE_INSTALL_INCLUDEDIR MUSIKCORE_INSTALL_RUNTIME_DIR)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/musikcoreConfigVersion.cmake"
  VERSION "${musikcube_VERSION}" COMPATIBILITY ExactVersion)

install(FILES "$<TARGET_FILE:musikcore>" DESTINATION "${MUSIKCORE_INSTALL_RUNTIME_DIR}" COMPONENT MusikcoreSDK)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/musikcore" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT MusikcoreSDK FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/musikcoreConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/musikcoreConfigVersion.cmake"
  DESTINATION "${musikcore_config_dir}" COMPONENT MusikcoreSDK)

# executable and shell script for musikcube
install(
  FILES "$<TARGET_FILE:musikcube>"
  DESTINATION share/musikcube
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/src/musikcube/musikcube"
  DESTINATION bin/
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ)

# executable and shell script for daemon
install(
  FILES "$<TARGET_FILE:musikcubed>"
  DESTINATION share/musikcube
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/src/musikcubed/musikcubed"
  DESTINATION bin/
  PERMISSIONS
    OWNER_EXECUTE OWNER_READ OWNER_WRITE
    GROUP_EXECUTE GROUP_READ GROUP_WRITE
    WORLD_EXECUTE WORLD_READ)
