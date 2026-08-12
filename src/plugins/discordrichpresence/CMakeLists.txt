set (discordrichpresence_SOURCES
    discord_plugin.cpp
)
set(DISCORD_SDK_VERSION 3.2.1)


add_library(discordrichpresence SHARED ${discordrichpresence_SOURCES})
if(NOT DISCORDGAMESDK)
    include(FetchContent)
    FetchContent_Declare(DiscordGameSDKDownload
        URL "https://dl-game-sdk.discordapp.net/${DISCORD_SDK_VERSION}/discord_game_sdk.zip"
    )
    FetchContent_MakeAvailable(DiscordGameSDKDownload)
    set(DISCORDGAMESDK "${discordgamesdkdownload_SOURCE_DIR}/lib/x86_64/discord_game_sdk.dll")
    include_directories(${discordgamesdkdownload_SOURCE_DIR}/c/)
endif()

if (${BUILD_STANDALONE} MATCHES "true")
    find_vendor_library(LIBCURL curl)
    include_directories(BEFORE ${VENDOR_INCLUDE_DIRECTORIES})
else()
    find_library(LIBCURL curl)
endif()

# Add Discord SDK include directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/discord_game_sdk/c)


message(STATUS "[discordrichpresence] using ${LIBCURL} ${DISCORDGAMESDK}")
target_link_libraries(discordrichpresence ${LIBCURL} ${DISCORDGAMESDK})
