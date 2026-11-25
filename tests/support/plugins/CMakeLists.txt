cmake_minimum_required(VERSION 3.16)
project(musikcore_external_provider LANGUAGES C CXX)

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

# rebuild the production C++ output using only relocated SDK headers. the
# generated C tables must not need private includes or link to musikcore.
find_package(Threads REQUIRED)
add_library(output_fixture MODULE "${OUTPUT_SOURCE}")
target_include_directories(output_fixture PRIVATE ${public_headers})
target_compile_definitions(output_fixture PRIVATE MC_BUILDING_PLUGIN)
target_compile_features(output_fixture PRIVATE cxx_std_17)
target_link_libraries(output_fixture PRIVATE Threads::Threads)
set_target_properties(output_fixture PROPERTIES CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/output-artifact-$<CONFIG>.txt"
  CONTENT "$<TARGET_FILE:output_fixture>")

# compile the media fixture as an external C decoder against installed headers.
add_library(decoder_fixture MODULE byte-decoder.c)
target_include_directories(decoder_fixture PRIVATE ${public_headers})
target_compile_definitions(decoder_fixture PRIVATE MC_BUILDING_PLUGIN)
set_target_properties(decoder_fixture PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  C_VISIBILITY_PRESET hidden SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/decoder-artifact-$<CONFIG>.txt"
  CONTENT "$<TARGET_FILE:decoder_fixture>")

add_executable(decoder_consumer decoder-consumer.cpp)
target_compile_features(decoder_consumer PRIVATE cxx_std_17)
target_link_libraries(decoder_consumer PRIVATE musikcore::musikcore Threads::Threads)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/decoder-consumer-$<CONFIG>.txt"
  CONTENT "$<TARGET_FILE:decoder_consumer>")
