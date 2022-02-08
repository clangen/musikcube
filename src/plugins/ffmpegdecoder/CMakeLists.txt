set (ffmpegdecoder_SOURCES
    plugin.cpp
    FfmpegDecoder.cpp
)

add_library(ffmpegdecoder SHARED ${ffmpegdecoder_SOURCES})

if (${LINK_STATICALLY} MATCHES "true")
    find_library(AVCODEC NAMES libavcodec-musikcube.a PATHS ${VENDOR_LINK_DIRECTORIES})
    find_library(AVUTIL NAMES libavutil-musikcube.a PATHS ${VENDOR_LINK_DIRECTORIES})
    find_library(AVFORMAT NAMES libavformat-musikcube.a PATHS ${VENDOR_LINK_DIRECTORIES})
    find_library(SWRESAMPLE NAMES libswresample-musikcube.a PATHS ${VENDOR_LINK_DIRECTORIES})
    target_link_libraries(ffmpegdecoder ${AVCODEC} ${AVUTIL} ${AVFORMAT} ${SWRESAMPLE})
else()
    # fedora (and probably other RPM-based distros) put ffmpeg includes here...
    include_directories("/usr/include/ffmpeg")
    include_directories("/usr/local/include/ffmpeg")
    ensure_library_exists(avcodec)
    ensure_library_exists(avutil)
    ensure_library_exists(avformat)
    ensure_library_exists(swresample)
    target_link_libraries(ffmpegdecoder avcodec avutil avformat swresample)
endif()
