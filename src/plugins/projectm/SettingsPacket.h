#pragma once

#include "Settings.h"
#include <cerrno>
#include <cstdint>
#include <type_traits>
#include <sys/socket.h>

/* macOS limits local datagrams to 2048 bytes. transfer one settings snapshot
in small ordered pieces over the existing socket, without delaying audio. */
struct ProjectMSettingsChunk {
    uint32_t offset;
    char data[1024];
};

static_assert(std::is_trivially_copyable_v<ProjectMSettings>);
static_assert(sizeof(ProjectMSettingsChunk) <= 2048);

inline bool SendProjectMSettings(int socket, const ProjectMSettings& settings, size_t& sent, int flags) noexcept {
    while (sent < sizeof(settings)) {
        ProjectMSettingsChunk chunk {};
        chunk.offset = static_cast<uint32_t>(sent);
        const auto count = std::min(sizeof(chunk.data), sizeof(settings) - sent);
        std::memcpy(chunk.data, reinterpret_cast<const char*>(&settings) + sent, count);
        const auto bytes = offsetof(ProjectMSettingsChunk, data) + count;
        if (send(socket, &chunk, bytes, flags) != static_cast<ssize_t>(bytes)) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        sent += count;
    }
    return true;
}

inline bool ReceiveProjectMSettings(
    const ProjectMSettingsChunk& chunk, size_t bytes, ProjectMSettings& settings, size_t& received) noexcept {
    const auto header = offsetof(ProjectMSettingsChunk, data);
    if (bytes <= header || bytes > sizeof(chunk)) {
        received = 0;
        return false;
    }
    if (chunk.offset == 0) {
        received = 0;
    }
    const auto count = bytes - header;
    if (chunk.offset != received || count > sizeof(settings) - received) {
        received = 0;
        return false;
    }
    std::memcpy(reinterpret_cast<char*>(&settings) + received, chunk.data, count);
    received += count;
    if (received == sizeof(settings)) {
        received = 0;
        NormalizeProjectMSettings(settings);
        return true;
    }
    return false;
}
