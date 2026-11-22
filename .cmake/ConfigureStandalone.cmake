if (${BUILD_STANDALONE} MATCHES "true")
  message(STATUS "${BoldGreen}[standalone-build] ENABLED!${ColorReset}")
  set(MUSIKCUBE_VENDOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor" CACHE PATH "Standalone vendor directory")

  list(
    APPEND
    VENDOR_INCLUDE_DIRECTORIES
    "${MUSIKCUBE_VENDOR_ROOT}/bin/include")

  list(
    APPEND
    VENDOR_LINK_DIRECTORIES
    "${MUSIKCUBE_VENDOR_ROOT}/bin/lib")

  include_directories(SYSTEM ${MUSIKCUBE_DEPENDENCY_INCLUDE_DIRS})
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${MUSIKCUBE_CXX_OPTIONS}>")

  foreach(archive IN LISTS MUSIKCUBE_REQUIRED_VENDOR_ARCHIVES)
    if (NOT EXISTS "${archive}")
      message(FATAL_ERROR "Missing vendor archive ${archive}. Rebuild the vendor libraries.")
    endif()
  endforeach()

else()
  message(STATUS "${BoldGreen}[standalone-build] *NOT* enabled!${ColorReset}")
endif()
