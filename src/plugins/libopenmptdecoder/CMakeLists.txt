set (openmptdecoder_SOURCES
  plugin.cpp
  OpenMptDecoder.cpp
  OpenMptIdexerSource.cpp
  Utility.cpp
)

add_library(openmptdecoder SHARED ${openmptdecoder_SOURCES})

# prefer static libraries on mac to make redist easier
if (${LINK_STATICALLY} MATCHES "true")
    find_library(OPENMPTLIB NAMES libopenmpt.a openmpt)
    target_link_libraries(openmptdecoder ${musikcube_LINK_LIBS} ${OPENMPTLIB})
else()
    target_link_libraries(openmptdecoder ${musikcube_LINK_LIBS} openmpt)
endif()

