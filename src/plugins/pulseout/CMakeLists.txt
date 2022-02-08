set (pulseout_SOURCES
  pulseout_plugin.cpp
  pulse_blocking_stream.c
  PulseOut.cpp
)

ensure_library_exists(pulse)

add_library(pulseout SHARED ${pulseout_SOURCES})
target_link_libraries(pulseout pulse)
