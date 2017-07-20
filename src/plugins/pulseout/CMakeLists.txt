set (pulseout_SOURCES
  pulseout_plugin.cpp
  pulse_blocking_stream.c
  PulseOut.cpp
)

add_definitions( 
  -D_DEBUG
)

add_library( pulseout SHARED ${pulseout_SOURCES} )
target_link_libraries( pulseout ${musikcube_LINK_LIBS} pulse)
