set (DAEMON_SRCS
	./main.cpp
)

add_executable(musikcubed ${DAEMON_SRCS})
target_link_libraries(musikcubed ${musikcube_LINK_LIBS} musikcore)
