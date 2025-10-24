set (discordrichpresence_SOURCES
    DiscordRichPresence.c
    plugin.cpp
)

add_library(discordrichpresence SHARED ${discordrichpresence_SOURCES})
find_library(DISCORD_GAME_SDK discord_game_sdk)


if (${BUILD_STANDALONE} MATCHES "true")
  find_vendor_library(LIBCURL libcurl)
  include_directories(BEFORE ${VENDOR_INCLUDE_DIRECTORIES})
else()
  find_library(LIBCURL libcurl)
endif()

message(STATUS "[discordrichpresence] using ${LIBCURL} ${DISCORD_GAME_SDK}")
target_link_libraries(discordrichpresence ${LIBCURL} ${DISCORD_GAME_SDK})
