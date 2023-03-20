set (gmedecoder_SOURCES
  plugin.cpp
  GmeDataStream.cpp
  GmeDecoder.cpp
  GmeIndexerSource.cpp
)

add_definitions(-DHAVE_STDINT_H)

if (${BUILD_STANDALONE} MATCHES "true")
  add_library(gmedecoder SHARED ${gmedecoder_SOURCES})
  include_directories(BEFORE ${VENDOR_INCLUDE_DIRECTORIES})
  find_vendor_library(GME gme)
  target_link_libraries(gmedecoder ${GME})
else()
  find_library(GME NAMES gme)
  if ("${GME}" STREQUAL "GME-NOTFOUND")
    disable_plugin(gmedecoder)
  else()
    add_library(gmedecoder SHARED ${gmedecoder_SOURCES})
    target_link_libraries(gmedecoder ${GME})
  endif()
endif()

message(STATUS "[gmedecoder] using ${GME}")
