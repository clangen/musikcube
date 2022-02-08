set (stockencoders_SOURCES
  main.cpp
  LameEncoder.cpp
  FfmpegEncoder.cpp
)

add_library(stockencoders SHARED ${stockencoders_SOURCES})

ensure_library_exists(mp3lame)

if (${LINK_STATICALLY} MATCHES "true")
  ensure_library_exists(avcodec-musikcube)
  ensure_library_exists(avutil-musikcube)
  ensure_library_exists(avformat-musikcube)
  ensure_library_exists(swresample-musikcube)
  target_link_libraries(stockencoders avcodec-musikcube avutil-musikcube avformat-musikcube swresample-musikcube)
else()
  # fedora (and probably other RPM-based distros) put ffmpeg includes here...
  include_directories("/usr/include/ffmpeg")
  include_directories("/usr/local/include/ffmpeg")
  ensure_library_exists(avcodec)
  ensure_library_exists(avutil)
  ensure_library_exists(avformat)
  ensure_library_exists(swresample)
  target_link_libraries(stockencoders ${musikcube_LINK_LIBS} avcodec avutil avformat swresample)
endif()

# prefer static libraries on mac to make redist easier
if (${LINK_STATICALLY} MATCHES "true")
  find_library(MP3LAMELIB NAMES libmp3lame.a)
  target_link_libraries(stockencoders ${MP3LAMELIB})
else()
  target_link_libraries(stockencoders mp3lame)
endif()

target_link_libraries(stockencoders ${musikcube_LINK_LIBS})
