set (CORE_C_DEMO_SRCS
  ./main.c
)

set(musikcube_INSTALL_DIR ${HOMEBREW_PREFIX})
if (NOT DEFINED musikcube_INSTALL_DIR)
	set(musikcube_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
endif()

add_executable(core_c_demo ${CORE_C_DEMO_SRCS})

set_target_properties(core_c_demo PROPERTIES LINK_FLAGS "-Wl,-rpath,./")

if (${LINK_STATICALLY} MATCHES "true")
    find_library(EVLIB NAMES libev.a ev)
    target_link_libraries(core_c_demo ${musikcube_LINK_LIBS} ${EVLIB} musikcore musikcore_c)
else()
	target_link_libraries(core_c_demo ${musikcube_LINK_LIBS} musikcore musikcore_c ev)
endif()
