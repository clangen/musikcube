set (macosmediakeys_SOURCES
  plugin.mm
  NSObject+SPInvocationGrabbing.m
  SPMediaKeyTap.m
)

if (APPLE)
  FIND_LIBRARY(APP_KIT_LIBRARY AppKit)
  FIND_LIBRARY(CARBON_LIBRARY Carbon)
  MARK_AS_ADVANCED (APP_KIT_LIBRARY CARBON_LIBRARY)
  add_library(macosmediakeys SHARED ${macosmediakeys_SOURCES})
  musikcube_configure_c_plugin(macosmediakeys)
  target_compile_features(macosmediakeys PRIVATE cxx_std_20)
  target_link_libraries(macosmediakeys musikcore ${APP_KIT_LIBRARY} ${CARBON_LIBRARY})
else()
  disable_plugin(macosmediakeys)
endif()