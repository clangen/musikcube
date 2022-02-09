set (DAEMON_SRCS
  ./main.cpp
)

set(musikcube_INSTALL_DIR ${HOMEBREW_PREFIX})
if (NOT DEFINED musikcube_INSTALL_DIR)
	set(musikcube_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
endif()

configure_file("musikcubed.in" "musikcubed" @ONLY)
add_executable(musikcubed ${DAEMON_SRCS})

target_include_directories(musikcubed BEFORE PRIVATE ${VENDOR_INCLUDE_DIRECTORIES})

if (${BUILD_STANDALONE} MATCHES "true")
    find_library(EVLIB NAMES libev.a ev)
else()
  find_library(EVLIB NAMES ev)
endif()

target_link_libraries(musikcubed ${musikcube_LINK_LIBS} ${EVLIB} musikcore)