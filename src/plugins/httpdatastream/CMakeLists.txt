set (httpdatastream_SOURCES
  httpdatastream_plugin.cpp
  HttpDataStream.cpp
  HttpDataStreamFactory.cpp
  LruDiskCache.cpp)

add_library(httpdatastream SHARED ${httpdatastream_SOURCES})

if (${BUILD_STANDALONE} MATCHES "true")
  include_directories(BEFORE ${VENDOR_INCLUDE_DIRECTORIES})
endif()

target_include_directories(httpdatastream BEFORE PUBLIC)
target_link_libraries(httpdatastream ${LIBCURL})
