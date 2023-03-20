set (stockencoders_SOURCES
  main.cpp
  LameEncoder.cpp
  FfmpegEncoder.cpp)

add_library(stockencoders SHARED ${stockencoders_SOURCES})

if (${BUILD_STANDALONE} MATCHES "true")
  include_directories(BEFORE ${VENDOR_INCLUDE_DIRECTORIES})
  find_vendor_library(AVCODEC avcodec-musikcube)
  find_vendor_library(AVUTIL avutil-musikcube)
  find_vendor_library(AVFORMAT avformat-musikcube)
  find_vendor_library(SWRESAMPLE  swresample-musikcube)
  find_vendor_library(MP3LAME mp3lame)
else()
  # fedora (and probably other RPM-based distros) put ffmpeg includes here...
  include_directories("/usr/include/ffmpeg")
  include_directories("/usr/local/include/ffmpeg")
  find_library(AVCODEC NAMES avcodec)
  find_library(AVUTIL NAMES avutil)
  find_library(AVFORMAT NAMES avformat)
  find_library(SWRESAMPLE NAMES swresample)
  find_library(MP3LAME NAMES mp3lame)
endif()

target_link_libraries(stockencoders ${AVCODEC} ${AVUTIL} ${AVFORMAT} ${SWRESAMPLE} ${MP3LAME})
