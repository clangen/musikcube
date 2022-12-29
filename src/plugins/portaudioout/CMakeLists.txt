set (portaudioout_SOURCES
  portaudioout_plugin.cpp
  PortAudioOut.cpp
)

find_library(LIBPORTAUDIO "portaudio")

if ("${LIBPORTAUDIO}" STREQUAL "LIBPORTAUDIO-NOTFOUND")
  disable_plugin(portaudioout)
else()
  add_library(portaudioout SHARED ${portaudioout_SOURCES})
  target_link_libraries(portaudioout ${LIBPORTAUDIO})
  message(STATUS "[portaudioout] using libportaudio at: ${LIBPORTAUDIO}")
endif()