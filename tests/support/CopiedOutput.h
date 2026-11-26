#pragma once
#include <cstddef>
#include <cstdint>

/* test-only control of a bounded output that copies and returns PCM before
its simulated device consumes it. time advances only when the test requests it. */
struct CopiedOutputSnapshot {
    uint64_t acceptedFrames;
    uint64_t playedFrames;
    uint64_t returnedBuffers;
    size_t queuedFrames;
    bool draining;
};
using ReadCopiedOutput = CopiedOutputSnapshot (*)();
using AdvanceCopiedOutput = void (*)(size_t);
using WaitCopiedOutput = bool (*)(uint64_t, unsigned);
