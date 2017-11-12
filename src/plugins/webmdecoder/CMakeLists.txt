set (webmdecoder_SOURCES
    stdafx.cpp
    webmdecoder_plugin.cpp
    debug.cpp
    IDataStreamReader.cpp
    WebmDecoderFactory.cpp
    WebmDecoder.cpp
    InternalOpusDecoder.cpp
)

add_library(webmdecoder SHARED ${webmdecoder_SOURCES})

target_link_libraries(webmdecoder ${BOOST_LINK_LIBS} opus webm.a vpx)
