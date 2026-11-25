cmake_minimum_required(VERSION 3.16)
project(musikcore_testing_disabled LANGUAGES C CXX)

# Exercise the real support directory without vendor or production dependencies.
set(BUILD_TESTING OFF)
find_package(Threads REQUIRED)
add_library(musikcore INTERFACE)
if(WIN32)
  add_library(musikcube_windows INTERFACE)
  add_library(vendor::ssl INTERFACE IMPORTED)
  add_custom_target(musikcube_runtime)
endif()
add_subdirectory("${SUPPORT}" support)

# Fixture executables/modules must never enter a tests-disabled target graph.
foreach(target IN ITEMS musikcore_runtime_hardening_test musikcore_legacy_entry_fixture musikcore_remote_query_transport_test musikcore_pcm_capture_output)
  if(TARGET ${target})
    message(FATAL_ERROR "Test target ${target} was created with BUILD_TESTING=OFF")
  endif()
endforeach()
