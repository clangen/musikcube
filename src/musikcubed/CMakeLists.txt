set (DAEMON_SRCS
  ./main.cpp
)

set(musikcube_INSTALL_DIR ${HOMEBREW_PREFIX})
if (NOT DEFINED musikcube_INSTALL_DIR)
	set(musikcube_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
endif()

configure_file("musikcubed.in" "musikcubed" @ONLY)

add_executable(musikcubed ${DAEMON_SRCS})
target_link_libraries(musikcubed ${musikcube_LINK_LIBS} musikcore)
set_target_properties(musikcubed PROPERTIES LINK_FLAGS "-Wl,-rpath,./")
