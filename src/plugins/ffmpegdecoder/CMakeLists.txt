# libresample2 libavcodec-extra57 libavutil55 libavformat57
# libavcodec-dev libavformat-dev libavutil-dev libswresample-dev

set (ffmpegdecoder_SOURCES
    plugin.cpp
    FfmpegDecoder.cpp
)

# note: static linking is a no-go (too many dependencies). sorry macOS.
add_library(ffmpegdecoder SHARED ${ffmpegdecoder_SOURCES})
target_link_libraries(ffmpegdecoder ${musikcube_LINK_LIBS} avcodec avutil avformat swresample)
