set (httpdatastream_SOURCES
  stdafx.cpp
  httpdatastream_plugin.cpp
  HttpDataStream.cpp
  HttpDataStreamFactory.cpp
  LruDiskCache.cpp
)

# add_definitions(-DFPM_DEFAULT)
# add_definitions(-DASO_ZEROCHECK)
# add_definitions(-DHAVE_CONFIG_H)

add_library(httpdatastream SHARED ${httpdatastream_SOURCES})

target_link_libraries(httpdatastream ${musikcube_LINK_LIBS} -lcurl)
