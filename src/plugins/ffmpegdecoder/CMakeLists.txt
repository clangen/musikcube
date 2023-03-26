set (ffmpegdecoder_SOURCES
    plugin.cpp
    FfmpegDecoder.cpp)

add_library(ffmpegdecoder SHARED ${ffmpegdecoder_SOURCES})

if (${BUILD_STANDALONE} MATCHES "true")
    include_directories(AFTER ${VENDOR_INCLUDE_DIRECTORIES})
    message(STATUS "[ffmpegdecoder] using include dirs: ${VENDOR_INCLUDE_DIRECTORIES}")
    find_vendor_library(AVCODEC avcodec-musikcube)
    find_vendor_library(AVUTIL avutil-musikcube)
    find_vendor_library(AVFORMAT avformat-musikcube)
    find_vendor_library(SWRESAMPLE swresample-musikcube)
else()
    # fedora (and probably other RPM-based distros) put ffmpeg includes here...
    include_directories("/usr/include/ffmpeg")
    include_directories("/usr/local/include/ffmpeg")
    find_library(AVCODEC NAMES avcodec)
    find_library(AVUTIL NAMES avutil)
    find_library(AVFORMAT NAMES avformat)
    find_library(SWRESAMPLE NAMES swresample)
endif()

target_link_libraries(ffmpegdecoder ${AVCODEC} ${AVUTIL} ${AVFORMAT} ${SWRESAMPLE})
