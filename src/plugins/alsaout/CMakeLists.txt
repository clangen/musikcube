set (alsaout_SOURCES
  alsaout_plugin.cpp
  AlsaOut.cpp
)

add_definitions( 
  -D_DEBUG
)

add_library( alsaout SHARED ${alsaout_SOURCES} )
target_link_libraries( alsaout ${musikcube_LINK_LIBS} asound)

