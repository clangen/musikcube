cmake_minimum_required(VERSION 3.16)
project(musikcore_testing_disabled LANGUAGES C CXX)

# Exercise the real support directory without vendor or production dependencies.
set(BUILD_TESTING OFF)
find_package(Threads REQUIRED)
add_library(musikcore INTERFACE)
add_subdirectory("${SUPPORT}" support)

# Fixture executables/modules must never enter a tests-disabled target graph.
foreach(target IN ITEMS musikcore_runtime_hardening_test musikcore_runtime_failure_First
    musikcore_runtime_failure_Second musikcore_remote_query_transport_test musikcore_pcm_capture_output)
  if(TARGET ${target})
    message(FATAL_ERROR "Test target ${target} was created with BUILD_TESTING=OFF")
  endif()
endforeach()
