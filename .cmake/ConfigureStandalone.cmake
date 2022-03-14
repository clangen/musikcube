if (${BUILD_STANDALONE} MATCHES "true")
  message(STATUS "${BoldGreen}[standalone-build] ENABLED!${ColorReset}")

  set(BOOST_ROOT "/build/musikcube/vendor/bin")
  set(BOOST_INCLUDEDIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/bin/include")
  set(BOOST_LIBRARYDIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/bin/lib")
  set(Boost_NO_SYSTEM_PATHS ON)
  set(Boost_NO_BOOST_CMAKE ON)

  list(
    APPEND
    VENDOR_INCLUDE_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/bin/include")

  list(
    APPEND
    VENDOR_LINK_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/bin/lib")

else()
  message(STATUS "${BoldGreen}[standalone-build] *NOT* enabled!${ColorReset}")
endif()
