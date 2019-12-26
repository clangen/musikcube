set (stockencoders_SOURCES
  main.cpp
  LameEncoder.cpp
  OggEncoder.cpp
  FfmpegEncoder.cpp
)

add_library(stockencoders SHARED ${stockencoders_SOURCES})

# prefer static libraries on mac to make redist easier
if (${LINK_STATICALLY} MATCHES "true")
  find_library(MP3LAMELIB NAMES libmp3lame.a mp3lame)
  find_library(OGGLIB NAMES libogg.a ogg)
  find_library(VORBISLIB NAMES libvorbis.a vorbis)
  find_library(VORBISENCLIB NAMES libvorbisenc.a vorbisenc)
  find_library(VORBISFILELIB NAMES libvorbisfile.a vorbisfile)
  target_link_libraries(stockencoders ${musikcube_LINK_LIBS} ${MP3LAMELIB} ${OGGLIB} ${VORBISLIB} ${VORBISENCLIB} ${VORBISFILELIB})
else()
  target_link_libraries(stockencoders ${musikcube_LINK_LIBS} ogg vorbis vorbisenc vorbisfile mp3lame)
endif()

# fedora (and probably other RPM-based distros) put ffmpeg includes here...
include_directories("/usr/include/ffmpeg")
include_directories("/usr/local/include/ffmpeg")

# note: static linking is a no-go (too many dependencies). sorry macOS.
target_link_libraries(stockencoders avcodec avutil avformat swresample)
