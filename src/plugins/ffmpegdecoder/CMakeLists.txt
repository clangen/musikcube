# libavformat-dev, libavcodec-dev, libavutil-dev, libavresample-dev(?), libswresample-dev

set (ffmpegdecoder_SOURCES
    plugin.cpp
    FfmpegDecoder.cpp
)

add_library(ffmpegdecoder SHARED ${ffmpegdecoder_SOURCES})

# prefer static libraries on mac to make redist easier
if (${LINK_STATICALLY} MATCHES "true")
    find_library(AVCODECLIB NAMES libavcodec.a avcodec)
    find_library(AVUTILLIB NAMES libavutil.a avutil)
    find_library(AVFORMATLIB NAMES libavformat.a avformat)
    find_library(SWRESAMPLELIB NAMES libswresample.a swresample)
    target_link_libraries(ffmpegdecoder ${musikcube_LINK_LIBS} ${AVCODECLIB} ${AVUTILLIB} ${AVFORMATLIB} ${SWRESAMPLELIB})
else()
    target_link_libraries(ffmpegdecoder ${musikcube_LINK_LIBS} avcodec avutil avformat swresample)
endif()
