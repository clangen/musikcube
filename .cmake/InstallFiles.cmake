# plugin dynamic libraries
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  file(GLOB plugins "bin/plugins/*.dylib")
  install(FILES ${plugins} DESTINATION share/musikcube/plugins)
else ()
  file(GLOB plugins "bin/plugins/*.so")
  install(FILES ${plugins} DESTINATION share/musikcube/plugins)
endif ()

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

# libmusikcore shared library
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  install(FILES "bin/libmusikcore.dylib" DESTINATION share/musikcube)
else()
  install(FILES "bin/libmusikcore.so" DESTINATION share/musikcube)
endif()

# executable and shell script for musikcube
install(
  FILES bin/musikcube
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
  FILES bin/musikcubed
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