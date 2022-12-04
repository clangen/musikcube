if (${BUILD_STANDALONE} MATCHES "true")
  message(STATUS "${BoldGreen}[standalone-build] ENABLED!${ColorReset}")

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
