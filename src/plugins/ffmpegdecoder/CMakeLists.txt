# libresample2 libavcodec-extra57 libavutil55 libavformat57
# libavcodec-dev libavformat-dev libavutil-dev libswresample-dev

set (ffmpegdecoder_SOURCES
    plugin.cpp
    FfmpegDecoder.cpp
)

# fedora (and probably other RPM-based distros) put ffmpeg includes here...
include_directories("/usr/include/ffmpeg")
include_directories("/usr/local/include/ffmpeg")

# note: static linking is a no-go (too many dependencies). sorry macOS.
add_library(ffmpegdecoder SHARED ${ffmpegdecoder_SOURCES})
target_link_libraries(ffmpegdecoder ${musikcube_LINK_LIBS} avcodec avutil avformat swresample)
