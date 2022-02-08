set (ffmpegdecoder_SOURCES
    plugin.cpp
    FfmpegDecoder.cpp)

add_library(ffmpegdecoder SHARED ${ffmpegdecoder_SOURCES})
target_include_directories(ffmpegdecoder PRIVATE BEFORE ${VENDOR_INCLUDE_DIRECTORIES})

if (${BUILD_STANDALONE} MATCHES "true")
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

target_link_libraries(ffmpegdecoder ${AVCODEC} ${AVUTIL} ${AVFORMAT} ${SWRESAMPLE})
