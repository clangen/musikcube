if(NOT APPLE)
  disable_plugin(coreaudioout)
  return()
endif()

find_library(CORE_FOUNDATION_LIBRARY CoreFoundation)
find_library(CORE_AUDIO_LIBRARY CoreAudio)
find_library(AUDIO_TOOLBOX_LIBRARY AudioToolbox)
mark_as_advanced(CORE_FOUNDATION_LIBRARY CORE_AUDIO_LIBRARY AUDIO_TOOLBOX_LIBRARY)

add_library(coreaudioout SHARED plugin.cpp CoreAudioOut.cpp)
musikcube_configure_c_plugin(coreaudioout)
target_compile_features(coreaudioout PRIVATE cxx_std_20)
target_link_libraries(coreaudioout PRIVATE
  ${CORE_FOUNDATION_LIBRARY}
  ${CORE_AUDIO_LIBRARY}
  ${AUDIO_TOOLBOX_LIBRARY}
  Threads::Threads)
