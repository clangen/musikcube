set (coreaudioout_SOURCES
  coreaudioout_plugin.cpp
  CoreAudioOut.cpp
)

IF(APPLE)
   FIND_LIBRARY(CORE_FOUNDATION_LIBRARY CoreFoundation)
   FIND_LIBRARY(CORE_AUDIO_LIBRARY CoreAudio)
   FIND_LIBRARY(AUDIO_TOOLBOX_LIBRARY AudioToolbox)
   MARK_AS_ADVANCED (CORE_FOUNDATION_LIBRARY
                     CORE_AUDIO_LIBRARY
                     AUDIO_TOOLBOX_LIBRARY)
   SET(FRAMEWORK_LIBS ${CORE_FOUNDATION_LIBRARY} ${CORE_AUDIO_LIBRARY} ${AUDIO_TOOLBOX_LIBRARY})
ENDIF (APPLE)

add_library(coreaudioout SHARED ${coreaudioout_SOURCES})
target_link_libraries(coreaudioout ${musikcube_LINK_LIBS} ${FRAMEWORK_LIBS})
