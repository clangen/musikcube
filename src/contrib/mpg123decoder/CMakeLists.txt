set (mpg123decoder_SOURCES
  stdafx.cpp
  mpg123decoder_plugin.cpp
  Mpg123DecoderFactory.cpp
  Mpg123Decoder.cpp
)

add_definitions(
  -DXML_STATIC
  -D_CRT_SECURE_NO_DEPRECATE
  -D_DEBUG
)

add_library(mpg123decoder SHARED ${mpg123decoder_SOURCES})
target_link_libraries(mpg123decoder ${musikbox_LINK_LIBS} mpg123)
