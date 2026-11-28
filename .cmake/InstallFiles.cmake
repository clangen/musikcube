if (${BUILD_STANDALONE} MATCHES "true")
  install(DIRECTORY "${MUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY}/lib/"
    DESTINATION share/musikcube/lib COMPONENT musikcoreSDK OPTIONAL)
  install(DIRECTORY "${MUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY}/share/"
    DESTINATION share/musikcube/share OPTIONAL)
endif()

# Use the production targets for both SDK installs and standalone archives.
get_property(runtime_plugins GLOBAL PROPERTY MUSIKCUBE_RUNTIME_PLUGINS)
set(plugin_manifest "")
foreach(plugin IN LISTS runtime_plugins)
  string(APPEND plugin_manifest "plugins/$<TARGET_FILE_NAME:${plugin}>\n")
  install(FILES "$<TARGET_FILE:${plugin}>"
    DESTINATION share/musikcube/plugins COMPONENT musikcorePlugins)
endforeach()
if(TARGET projectm_helper)
  string(APPEND plugin_manifest "plugins/$<TARGET_FILE_NAME:projectm_helper>\n")
endif()
file(GENERATE OUTPUT "${PROJECT_BINARY_DIR}/CMakeFiles/runtime-plugins-$<CONFIG>.txt" CONTENT "${plugin_manifest}")

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
set(MUSIKCORE_INSTALL_RUNTIME_DIR share/musikcube)
include("${CMAKE_CURRENT_LIST_DIR}/InstallMusikcoreSdk.cmake")

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

if(APPLE)
  # run after all copies, for full installs as well as either SDK component.
  foreach(component IN ITEMS musikcoreSDK musikcorePlugins)
    install(CODE "set(MUSIKCORE_RUNTIME \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/share/musikcube\")
include(\"${PROJECT_SOURCE_DIR}/.cmake/FixupMachORpaths.cmake\")"
      COMPONENT ${component})
  endforeach()
endif()
