set (DAEMON_SRCS
  ./main.cpp
)

set(musikcube_INSTALL_DIR ${HOMEBREW_PREFIX})
if (NOT DEFINED musikcube_INSTALL_DIR)
	set(musikcube_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
endif()

configure_file("musikcubed.in" "musikcubed" @ONLY)
add_executable(musikcubed ${DAEMON_SRCS})

set_target_properties(musikcubed PROPERTIES LINK_FLAGS "-Wl,-rpath,./")

if (${LINK_STATICALLY} MATCHES "true")
    find_library(EVLIB NAMES libev.a ev)
    target_link_libraries(musikcubed ${musikcube_LINK_LIBS} ${EVLIB} musikcore)
else()
	target_link_libraries(musikcubed ${musikcube_LINK_LIBS} musikcore ev)
endif()
