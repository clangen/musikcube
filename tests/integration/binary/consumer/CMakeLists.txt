cmake_minimum_required(VERSION 3.16)
project(musikcore_installed_consumers LANGUAGES C CXX)

find_package(musikcore CONFIG REQUIRED)
find_package(Threads REQUIRED)

# instrumented SDK consumers must link their compiler runtime themselves;
# relying on a toolchain RPATH embedded in the SDK breaks relocation.
if(CONSUMER_SANITIZERS)
  add_compile_options("$<$<CONFIG:Debug>:-fsanitize=${CONSUMER_SANITIZERS}>"
    "$<$<CONFIG:Debug>:-fno-sanitize-recover=all>")
  add_link_options("$<$<CONFIG:Debug>:-fsanitize=${CONSUMER_SANITIZERS}>")
endif()

foreach(language IN ITEMS c cpp)
  add_executable(query_${language} "${EXAMPLES}/core-query.${language}" "${SUPPORT}/host-wakeup.cpp")
  target_include_directories(query_${language} PRIVATE "${SUPPORT}")
  target_link_libraries(query_${language} PRIVATE musikcore::musikcore Threads::Threads)
  set_target_properties(query_${language} PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endforeach()

# compile both creation modes against the installed public SDK only.
foreach(language IN ITEMS c cpp)
  add_executable(engine_${language} "${EXAMPLES}/core-engine.${language}" "${SUPPORT}/host-wakeup.cpp")
  target_include_directories(engine_${language} PRIVATE "${SUPPORT}")
  target_link_libraries(engine_${language} PRIVATE musikcore::musikcore Threads::Threads)
  set_target_properties(engine_${language} PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endforeach()

foreach(language IN ITEMS c cpp)
  add_executable(player_${language} "${EXAMPLES}/core-player.${language}" "${SUPPORT}/host-wakeup.cpp")
  target_include_directories(player_${language} PRIVATE "${SUPPORT}")
  target_link_libraries(player_${language} PRIVATE musikcore::musikcore Threads::Threads)
  set_target_properties(player_${language} PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endforeach()

add_executable(managed managed.c)
target_link_libraries(managed PRIVATE musikcore::musikcore)
set_target_properties(managed PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)

add_executable(session session.c "${SUPPORT}/host-wakeup.cpp")
target_include_directories(session PRIVATE "${SUPPORT}")
target_link_libraries(session PRIVATE musikcore::musikcore Threads::Threads)
set_target_properties(session PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
