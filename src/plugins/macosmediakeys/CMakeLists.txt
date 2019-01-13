set (macosmediakeys_SOURCES
  plugin.mm
  NSObject+SPInvocationGrabbing.m
  SPMediaKeyTap.m
)

FIND_LIBRARY(APP_KIT_LIBRARY AppKit)
FIND_LIBRARY(CARBON_LIBRARY Carbon)
MARK_AS_ADVANCED (APP_KIT_LIBRARY CARBON_LIBRARY)

add_library(macosmediakeys SHARED ${macosmediakeys_SOURCES})
target_link_libraries(macosmediakeys ${musikcube_LINK_LIBS} ${APP_KIT_LIBRARY} ${CARBON_LIBRARY})
