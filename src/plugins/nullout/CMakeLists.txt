set (nullout_SOURCES
  nullout_plugin.cpp
  NullOut.cpp
)

add_library(nullout SHARED ${nullout_SOURCES})
target_link_libraries(nullout ${musikcube_LINK_LIBS})
