if (${BUILD_STANDALONE} MATCHES "true")
  message(STATUS "[standalone-build] ENABLED!")

  set(BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-bin")
  set(BOOST_INCLUDEDIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-bin/include")
  set(BOOST_LIBRARYDIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-bin/lib")
  set(Boost_NO_SYSTEM_PATHS ON)
  set(Boost_NO_BOOST_CMAKE ON)

  list(
    APPEND
    VENDOR_INCLUDE_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ffmpeg-bin/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/lame-bin/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libmicrohttpd-bin/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/zlib-bin/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/curl-bin/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ncurses-bin/include")

  list(
    APPEND
    VENDOR_LINK_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ffmpeg-bin/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/lame-bin/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libmicrohttpd-bin/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/zlib-bin/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/curl-bin/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ncurses-bin/lib")
else()
    message(STATUS "[standalone-build] *NOT* enabled!")
endif()