set(CORE_C_SOURCES
  context.cpp
  interface_wrappers.cpp
)

include_directories(
  .
  ..
)

add_definitions(-DDEFINE_EXPORTS)

add_library(musikcore_c SHARED ${CORE_C_SOURCES})

set_target_properties(musikcore_c PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${musikcube_SOURCE_DIR}/bin)

if (${LINK_STATICALLY} MATCHES "true")
  find_library(EVLIB NAMES libev.a ev)
  target_link_libraries(musikcore_c ${musikcube_LINK_LIBS} ${EVLIB})
else()
  target_link_libraries(musikcore_c ${musikcube_LINK_LIBS} ev)
endif()
