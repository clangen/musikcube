set (coreaudioout_SOURCES
  coreaudioout_plugin.cpp
  CoreAudioOut.cpp
)

if (APPLE)
  find_library(CORE_FOUNDATION_LIBRARY CoreFoundation)
  find_library(CORE_AUDIO_LIBRARY CoreAudio)
  find_library(AUDIO_TOOLBOX_LIBRARY AudioToolbox)
  mark_as_advanced(CORE_FOUNDATION_LIBRARY
                   CORE_AUDIO_LIBRARY
                   AUDIO_TOOLBOX_LIBRARY)
  set(FRAMEWORK_LIBS ${CORE_FOUNDATION_LIBRARY} ${CORE_AUDIO_LIBRARY} ${AUDIO_TOOLBOX_LIBRARY})

  add_library(coreaudioout SHARED ${coreaudioout_SOURCES})
  target_link_libraries(coreaudioout ${FRAMEWORK_LIBS})
else()
  disable_plugin(coreaudioout)
endif()
