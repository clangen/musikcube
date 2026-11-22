include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(MUSIKCORE_LIBRARY_FILENAME "${CMAKE_SHARED_LIBRARY_PREFIX}musikcore${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(MUSIKCORE_IMPORT_FILENAME "${CMAKE_IMPORT_LIBRARY_PREFIX}musikcore${CMAKE_IMPORT_LIBRARY_SUFFIX}")
set(musikcore_config_dir "${CMAKE_INSTALL_LIBDIR}/cmake/musikcore")
configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/musikcoreConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/musikcoreConfig.cmake"
  INSTALL_DESTINATION "${musikcore_config_dir}"
  PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR MUSIKCORE_INSTALL_RUNTIME_DIR)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/musikcoreConfigVersion.cmake"
  VERSION "${musikcube_VERSION}" COMPATIBILITY ExactVersion)

install(FILES "$<TARGET_FILE:musikcore>" DESTINATION "${MUSIKCORE_INSTALL_RUNTIME_DIR}" COMPONENT musikcoreSDK)
if(WIN32)
  install(FILES "$<TARGET_LINKER_FILE:musikcore>" DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT musikcoreSDK)
endif()
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/musikcore" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT musikcoreSDK FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/musikcoreConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/musikcoreConfigVersion.cmake"
  DESTINATION "${musikcore_config_dir}" COMPONENT musikcoreSDK)
