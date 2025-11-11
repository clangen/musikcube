cmake_minimum_required(VERSION 3.10)
project(discord)

set(SRC
    discord_plugin.cpp
)

add_library(discord SHARED ${SRC})

target_include_directories(discord PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/musikcore/include
)

target_link_libraries(discord
    musikcore
)

set_target_properties(discord PROPERTIES
    PREFIX ""
    OUTPUT_NAME "discord"
)
