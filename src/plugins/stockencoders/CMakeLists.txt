set (stockencoders_SOURCES
  main.cpp
  LameEncoder.cpp
  FfmpegEncoder.cpp
)

add_library(stockencoders SHARED ${stockencoders_SOURCES})

ensure_library_exists(mp3lame)

if (${LINK_STATICALLY} MATCHES "true")
  find_library(AVCODEC NAMES libavcodec-musikcube.a PATHS ${VENDOR_LINK_DIRECTORIES})
  find_library(AVUTIL NAMES libavutil-musikcube.a PATHS ${VENDOR_LINK_DIRECTORIES})
  find_library(AVFORMAT NAMES libavformat-musikcube.a PATHS ${VENDOR_LINK_DIRECTORIES})
  find_library(SWRESAMPLE NAMES libswresample-musikcube.a PATHS ${VENDOR_LINK_DIRECTORIES})
  target_link_libraries(stockencoders ${AVCODEC} ${AVUTIL} ${AVFORMAT} ${SWRESAMPLE})
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
