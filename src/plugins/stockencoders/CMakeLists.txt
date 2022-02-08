set (stockencoders_SOURCES
  main.cpp
  LameEncoder.cpp
  FfmpegEncoder.cpp)

add_library(stockencoders SHARED ${stockencoders_SOURCES})

if (${BUILD_STANDALONE} MATCHES "true")
  target_include_directories(stockencoders BEFORE PRIVATE ${VENDOR_INCLUDE_DIRECTORIES})
  find_library(AVCODEC NAMES avcodec-musikcube PATHS ${VENDOR_LINK_DIRECTORIES} NO_DEFAULT_PATH)
  find_library(AVUTIL NAMES avutil-musikcube PATHS ${VENDOR_LINK_DIRECTORIES} NO_DEFAULT_PATH)
  find_library(AVFORMAT NAMES avformat-musikcube PATHS ${VENDOR_LINK_DIRECTORIES} NO_DEFAULT_PATH)
  find_library(SWRESAMPLE NAMES swresample-musikcube PATHS ${VENDOR_LINK_DIRECTORIES} NO_DEFAULT_PATH)
else()
  # fedora (and probably other RPM-based distros) put ffmpeg includes here...
  include_directories("/usr/include/ffmpeg")
  include_directories("/usr/local/include/ffmpeg")
  find_library(AVCODEC NAMES avcodec)
  find_library(AVUTIL NAMES avutil)
  find_library(AVFORMAT NAMES avformat)
  find_library(SWRESAMPLE NAMES swresample)
endif()

find_library(MP3LAME NAMES mp3lame)
target_link_libraries(stockencoders ${AVCODEC} ${AVUTIL} ${AVFORMAT} ${SWRESAMPLE} ${MP3LAME})
