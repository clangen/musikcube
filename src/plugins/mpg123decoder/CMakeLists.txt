set (mpg123decoder_SOURCES
  stdafx.cpp
  mpg123decoder_plugin.cpp
  Mpg123DecoderFactory.cpp
  Mpg123Decoder.cpp
)

add_library(mpg123decoder SHARED ${mpg123decoder_SOURCES})
target_link_libraries(mpg123decoder ${musikcube_LINK_LIBS} mpg123)
