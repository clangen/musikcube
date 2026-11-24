set (pulseout_SOURCES
  pulseout_plugin.cpp
  pulse_blocking_stream.c
  PulseOut.cpp
)

find_library(LIBPULSE pulse)

if ("${LIBPULSE}" STREQUAL "LIBPULSE-NOTFOUND")
  disable_plugin(pulseout)
else()
  add_library(pulseout SHARED ${pulseout_SOURCES})
  musikcube_configure_c_plugin(pulseout)
  target_compile_features(pulseout PRIVATE cxx_std_20)
  target_link_libraries(pulseout ${LIBPULSE})
endif()
