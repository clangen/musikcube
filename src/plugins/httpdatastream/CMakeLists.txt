set (httpdatastream_SOURCES
  httpdatastream_plugin.cpp
  HttpDataStream.cpp
  LruDiskCache.cpp)

add_library(httpdatastream SHARED ${httpdatastream_SOURCES})
musikcube_configure_c_plugin(httpdatastream)
target_compile_features(httpdatastream PRIVATE cxx_std_20)

if (${BUILD_STANDALONE} MATCHES "true")
  include_directories(BEFORE ${VENDOR_INCLUDE_DIRECTORIES})
endif()

target_link_libraries(httpdatastream ${LIBCURL})
