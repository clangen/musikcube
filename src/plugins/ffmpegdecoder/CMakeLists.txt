set (ffmpegdecoder_SOURCES
    plugin.cpp
    FfmpegDecoder.cpp
)

add_library(ffmpegdecoder SHARED ${ffmpegdecoder_SOURCES})

if (${LINK_STATICALLY} MATCHES "true")
    ensure_library_exists(avcodec-musikcube)
    ensure_library_exists(avutil-musikcube)
    ensure_library_exists(avformat-musikcube)
    ensure_library_exists(swresample-musikcube)
    target_link_libraries(ffmpegdecoder avcodec-musikcube avutil-musikcube avformat-musikcube swresample-musikcube)
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

#target_link_libraries(ffmpegdecoder ${musikcube_LINK_LIBS})
