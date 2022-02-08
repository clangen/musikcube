if (${LINK_STATICALLY} MATCHES "true")
  message(STATUS "[static linking] enabling static linking...")

  set(BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-bin")
  set(BOOST_INCLUDEDIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-bin/include")
  set(BOOST_LIBRARYDIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-bin/lib")
  set(Boost_NO_SYSTEM_PATHS ON)
  #set(Boost_USE_STATIC_LIBS ON)
  set(Boost_NO_BOOST_CMAKE ON)

  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/ffmpeg-bin/include")
  link_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/ffmpeg-bin/lib")

  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/lame-bin/include")
  link_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/lame-bin/lib")

  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/libmicrohttpd-bin/include")
  link_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/libmicrohttpd-bin/lib")

  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/zlib-bin/include")
  link_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/zlib-bin/lib")

  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/curl-bin/include")
  link_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/curl-bin/lib")
else()
    message(STATUS "[static linking] static linking NOT enabled!")
endif()