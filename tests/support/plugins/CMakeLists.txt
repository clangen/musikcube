cmake_minimum_required(VERSION 3.16)
project(musikcore_external_provider LANGUAGES C)

# obtain public include paths from the relocated package, without linking it.
find_package(musikcore CONFIG REQUIRED)
get_target_property(public_headers musikcore::musikcore INTERFACE_INCLUDE_DIRECTORIES)
add_library(provider_fixture MODULE provider.c)
target_include_directories(provider_fixture PRIVATE ${public_headers})
target_compile_definitions(provider_fixture PRIVATE MC_BUILDING_PLUGIN)
set_target_properties(provider_fixture PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET hidden SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/artifact-$<CONFIG>.txt"
  CONTENT "$<TARGET_FILE:provider_fixture>")
