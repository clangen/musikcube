set (httpdatastream_SOURCES
  httpdatastream_plugin.cpp
  HttpDataStream.cpp
  HttpDataStreamFactory.cpp
  LruDiskCache.cpp
)

add_library(httpdatastream SHARED ${httpdatastream_SOURCES})
target_link_libraries(httpdatastream ${musikcube_LINK_LIBS})
