set (externalart_SOURCES
  externalart_plugin.cpp
  ExternalArtReader.cpp
)

add_library(externalart SHARED ${externalart_SOURCES})