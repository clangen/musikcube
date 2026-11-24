set (taglibreader_SOURCES
  taglib_plugin.cpp
  TaglibMetadataReader.cpp
)

add_library(taglibreader SHARED ${taglibreader_SOURCES})
musikcube_configure_c_plugin(taglibreader)

find_library(LIBZ z)

if (${BUILD_STANDALONE} MATCHES "true")
  find_vendor_library(LIBTAG tag)
  include_directories(BEFORE ${VENDOR_INCLUDE_DIRECTORIES})
else()
  find_library(LIBTAG tag)
endif()

message(STATUS "[taglibmetadatareader] using ${LIBTAG}")
target_link_libraries(taglibreader ${LIBTAG} ${LIBZ})
