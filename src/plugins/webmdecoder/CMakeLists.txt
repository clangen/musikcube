set (webmdecoder_SOURCES
    stdafx.cpp
    webmdecoder_plugin.cpp
    IDataStreamReader.cpp
    CallbackHandler.cpp
    WebmDecoderFactory.cpp
    WebmDecoder.cpp
)

add_library(webmdecoder SHARED ${webmdecoder_SOURCES})

target_link_libraries(webmdecoder ${BOOST_LINK_LIBS} webm.a vpx)
