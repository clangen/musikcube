#pragma once

#include <cstddef>
#include <cstdint>

/* small datagrams keep audio delivery nonblocking even if the window stalls. */
struct ProjectMPcmPacket {
    static constexpr size_t MaxFrames = 192;
    uint32_t frames;
    float samples[MaxFrames * 2];
};
