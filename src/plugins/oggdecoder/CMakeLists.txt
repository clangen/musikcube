set (oggdecoder_SOURCES
    oggdecoder_plugin.cpp
    OggDecoder.cpp
    OggDecoderFactory.cpp
    stdafx.cpp
)

ensure_library_exists(ogg)
ensure_library_exists(vorbis)
ensure_library_exists(vorbisfile)

add_library(oggdecoder SHARED ${oggdecoder_SOURCES})

# prefer static libraries on mac to make redist easier
if (${LINK_STATICALLY} MATCHES "true")
    ensure_library_exists(libogg.a)
    ensure_library_exists(libvorbisfile.a)
    ensure_library_exists(libvorbis.a)
    find_library(OGGLIB NAMES libogg.a ogg)
    find_library(VORBISLIB NAMES libvorbis.a vorbis)
    find_library(VORBISFILELIB NAMES libvorbisfile.a vorbisfile)
    target_link_libraries(oggdecoder ${musikcube_LINK_LIBS} ${OGGLIB} ${VORBISLIB} ${VORBISFILELIB})
else()
    target_link_libraries(oggdecoder ${musikcube_LINK_LIBS} ogg vorbis vorbisfile)
endif()

