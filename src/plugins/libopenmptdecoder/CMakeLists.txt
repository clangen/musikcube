set (openmptdecoder_SOURCES
  plugin.cpp
  OpenMptDataStream.cpp
  OpenMptDecoder.cpp
  OpenMptIndexerSource.cpp
  Utility.cpp)

if (${BUILD_STANDALONE} MATCHES "true")
  add_library(openmptdecoder SHARED ${openmptdecoder_SOURCES})
  target_include_directories(openmptdecoder BEFORE PRIVATE ${VENDOR_INCLUDE_DIRECTORIES})
  find_vendor_library(LIBOPENMPT openmpt)
  target_link_libraries(openmptdecoder ${LIBOPENMPT})
else()
  find_library(LIBOPENMPT NAMES openmpt)
  find_library(LIBMPG123 NAMES mpg123)
  find_library(LIBZ NAMES z)
  if ("${LIBOPENMPT}" STREQUAL "LIBOPENMPT-NOTFOUND")
    disable_plugin(openmptdecoder)
  else()
    add_library(openmptdecoder SHARED ${openmptdecoder_SOURCES})
    target_link_libraries(openmptdecoder ${LIBOPENMPT} ${LIBMPG123} ${LIBZ})
  endif()
endif()


