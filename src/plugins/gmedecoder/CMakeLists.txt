set (gmedecoder_SOURCES
  plugin.cpp
  GmeDataStream.cpp
  GmeDecoder.cpp
  GmeIndexerSource.cpp
)

add_library(gmedecoder SHARED ${gmedecoder_SOURCES})

if (${BUILD_STANDALONE} MATCHES "true")
    add_vendor_includes(gmedecoder)
    find_vendor_library(GME gme)
else()
    find_library(GME NAMES gme)
endif()

message(STATUS "[gmedecoder] using ${GME}")
add_definitions(-DHAVE_STDINT_H)
target_link_libraries(gmedecoder ${GME})
