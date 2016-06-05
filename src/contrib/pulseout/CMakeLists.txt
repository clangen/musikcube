set (pulseout_SOURCES
  pulseout_plugin.cpp
  PulseOut.cpp
)

add_library(pulseout SHARED ${pulseout_SOURCES})
target_link_libraries(pulseout ${musikbox_LINK_LIBS} pulse)
