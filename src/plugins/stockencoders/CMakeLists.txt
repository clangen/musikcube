set (stockencoders_SOURCES
  main.cpp
  LameEncoder.cpp
  FfmpegEncoder.cpp
)

add_library(stockencoders SHARED ${stockencoders_SOURCES})

if (${FFMPEG_ENABLED} MATCHES "false")
  message(STATUS "[stockencoders] *not* defining FFMPEG_ENABLED")
else()
  message(STATUS "[stockencoders] defining FFMPEG_ENABLED")
  add_definitions(-DFFMPEG_ENABLED)
  # fedora (and probably other RPM-based distros) put ffmpeg includes here...
  include_directories("/usr/include/ffmpeg")
  include_directories("/usr/local/include/ffmpeg")
  # note: static linking is a no-go (too many dependencies). sorry macOS.
  target_link_libraries(stockencoders avcodec avutil avformat swresample)
endif()

# prefer static libraries on mac to make redist easier
if (${LINK_STATICALLY} MATCHES "true")
  find_library(MP3LAMELIB NAMES libmp3lame.a mp3lame)
  target_link_libraries(stockencoders ${musikcube_LINK_LIBS} ${MP3LAMELIB})
else()
  target_link_libraries(stockencoders ${musikcube_LINK_LIBS} mp3lame)
endif()
