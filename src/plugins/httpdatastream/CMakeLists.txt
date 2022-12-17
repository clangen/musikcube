set (httpdatastream_SOURCES
  httpdatastream_plugin.cpp
  HttpDataStream.cpp
  HttpDataStreamFactory.cpp
  LruDiskCache.cpp)

add_library(httpdatastream SHARED ${httpdatastream_SOURCES})

add_vendor_includes(httpdatastream)
target_include_directories(httpdatastream BEFORE PUBLIC)
target_link_libraries(httpdatastream ${LIBCURL})
