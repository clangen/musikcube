set (ffmpegdecoder_SOURCES
    plugin.cpp
    FfmpegDecoder.cpp
)

add_library(ffmpegdecoder SHARED ${ffmpegdecoder_SOURCES})

if (${LINK_STATICALLY} MATCHES "true")
    find_library(LIBAVCODEC NAMES libavcodec-musikcube.a PATHS "/home/clangen/src/musikcube/vendor/ffmpeg-bin/lib/")
    find_library(LIBAVUTIL NAMES libavutil-musikcube.a PATHS "/home/clangen/src/musikcube/vendor/ffmpeg-bin/lib/")
    find_library(LIBAVFORMAT NAMES libavformat-musikcube.a PATHS "/home/clangen/src/musikcube/vendor/ffmpeg-bin/lib/")
    find_library(LIBSWRESAMPLE NAMES libswresample-musikcube.a PATHS "/home/clangen/src/musikcube/vendor/ffmpeg-bin/lib/")
    target_link_libraries(ffmpegdecoder ${LIBAVCODEC} ${LIBAVUTIL} ${LIBAVFORMAT} ${LIBSWRESAMPLE})
    #target_link_libraries(ffmpegdecoder -Wl,-Bdynamic libavcodec-musikcube.a libavutil-musikcube.a libavformat-musikcube.a libswresample-musikcube.a)

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

message(STATUS "clclcl ${LIBAVCODEC} ${LIBAVUTIL} ${LIBAVFORMAT} ${LIBSWRESAMPLE}")

#target_link_libraries(ffmpegdecoder ${musikcube_LINK_LIBS})
