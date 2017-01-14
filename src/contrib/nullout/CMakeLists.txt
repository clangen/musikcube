set (nullout_SOURCES
  nullout_plugin.cpp
  NullOut.cpp
)

add_library(nullout SHARED ${alsaout_SOURCES})
target_link_libraries(nullout ${musikbox_LINK_LIBS})
