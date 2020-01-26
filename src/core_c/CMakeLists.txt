set(CORE_C_SOURCES
  musikcore_c.cpp
)

include_directories(
  .
  ..
)

add_library(musikcore_c SHARED ${CORE_C_SOURCES})

set_target_properties(musikcore_c PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${musikcube_SOURCE_DIR}/bin)

target_link_libraries(musikcore_c ${musikcube_LINK_LIBS})
