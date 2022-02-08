set (openmptdecoder_SOURCES
  plugin.cpp
  OpenMptDataStream.cpp
  OpenMptDecoder.cpp
  OpenMptIndexerSource.cpp
  Utility.cpp
)

ensure_library_exists(openmpt)

add_library(openmptdecoder SHARED ${openmptdecoder_SOURCES})

# prefer static libraries on mac to make redist easier
if (${LINK_STATICALLY} MATCHES "true")
    ensure_library_exists(mpg123)
    find_library(OPENMPTLIB NAMES libopenmpt.a openmpt)
    find_library(MPG123LIB NAMES libmpg123.a mpg123)
    find_library(ZLIB NAMES libz.a z)
    message(STATUS "[libopenmptdecoder] using ${OPENMPTLIB}")
    message(STATUS "[libopenmptdecoder] using ${MPG123LIB}")
    message(STATUS "[libopenmptdecoder] using ${ZLIB}")
    target_link_libraries(openmptdecoder ${OPENMPTLIB} ${MPG123LIB} ${ZLIB})
else()
    target_link_libraries(openmptdecoder openmpt)
endif()
