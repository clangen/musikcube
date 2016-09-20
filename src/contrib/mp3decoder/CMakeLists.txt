set (mp3decoder_SOURCES
  BaseDecoder.cpp
  BitStream.cpp
  CRC.cpp
  Header.cpp
  Layer3Decoder.cpp
  mp3decoder_plugin.cpp
  Mp3Decoder.cpp
  MP3DecoderFactory.cpp
  SideInfo.cpp
  stdafx.cpp
)

add_definitions(
  -D_DEBUG
)

add_library(mp3decoder SHARED ${mp3decoder_SOURCES})
target_link_libraries(mp3decoder ${musikbox_LINK_LIBS})
