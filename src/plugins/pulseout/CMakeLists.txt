set (pulseout_SOURCES
  pulseout_plugin.cpp
  pulse_blocking_stream.c
  PulseOut.cpp
)

add_library(pulseout SHARED ${pulseout_SOURCES})

find_library(LIBPULSE pulse)
target_link_libraries(pulseout ${LIBPULSE})
