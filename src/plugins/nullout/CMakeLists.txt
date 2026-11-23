add_library(nullout SHARED plugin.cpp)
musikcube_configure_c_plugin(nullout)
target_compile_features(nullout PRIVATE cxx_std_17)
target_link_libraries(nullout PRIVATE Threads::Threads)
