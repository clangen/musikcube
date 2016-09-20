set (oggdecoder_SOURCES
    oggdecoder_plugin.cpp
    OggDecoder.cpp
    OggDecoderFactory.cpp
    stdafx.cpp
)

add_library(oggdecoder SHARED ${oggdecoder_SOURCES})
target_link_libraries(oggdecoder ${musikbox_LINK_LIBS} ogg vorbis vorbisfile)
