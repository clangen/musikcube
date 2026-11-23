add_library(nullout SHARED plugin.cpp)

# public headers define the function tables; Threads::Threads supports the interruptible producer wait.
target_include_directories(nullout PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(nullout PRIVATE MC_BUILDING_PLUGIN)
target_compile_features(nullout PRIVATE cxx_std_17)
target_link_libraries(nullout PRIVATE Threads::Threads)

# add_plugin uses this property for the export allowlist. all other functions
# are reached through the tables returned by mc_plugin_get_api.
set_target_properties(nullout PROPERTIES
  MUSIKCUBE_PLUGIN_ENTRY mc_plugin_get_api
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)
