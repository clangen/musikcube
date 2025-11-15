set (discordrichpresence_SOURCES
    discord_plugin.cpp
    DiscordRichPresence.cpp
)

add_library(discordrichpresence SHARED ${discordrichpresence_SOURCES})

find_vendor_library(DISCORDGAMESDK discord_game_sdk)

if (${BUILD_STANDALONE} MATCHES "true")
    find_vendor_library(LIBCURL curl)
    include_directories(BEFORE ${VENDOR_INCLUDE_DIRECTORIES})
else()
    find_library(LIBCURL curl)
endif()

message(STATUS "[discordrichpresence] using ${LIBCURL} ${DISCORDGAMESDK}")
target_link_libraries(discordrichpresence ${LIBCURL} ${DISCORDGAMESDK})
