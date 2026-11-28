#include <plugins/sndioout/SndioOut.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::fprintf(stderr, "sndio timing test failed at line %d: %s\n", __LINE__, #condition);                   \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (false)

namespace {
    sio_par negotiated {};
    void (*moveCallback)(void*, int) = nullptr;
    void* moveContext = nullptr;
    bool firstWrite = true;
    int handle = 0;
    unsigned returned = 0;

    mc_output_timing timingOf(const SndioOut& output) {
        auto timing = mc_output_timing MC_OUTPUT_TIMING_DEFAULT;
        CHECK(output.GetTiming(timing) == MC_OK);
        return timing;
    }
}

extern "C" {
sio_hdl* sio_open(const char*, unsigned, int) {
    return reinterpret_cast<sio_hdl*>(&handle);
}
void sio_close(sio_hdl*) {
}
void sio_initpar(sio_par* parameters) {
    std::memset(parameters, 0, sizeof(*parameters));
}
int sio_setpar(sio_hdl*, sio_par* parameters) {
    negotiated = *parameters;
    return 1;
}
int sio_getpar(sio_hdl*, sio_par* parameters) {
    *parameters = negotiated;
    return 1;
}
int sio_nfds(sio_hdl*) {
    return 0;
}
void sio_onmove(sio_hdl*, void (*callback)(void*, int), void* context) {
    moveCallback = callback;
    moveContext = context;
}
int sio_start(sio_hdl*) {
    firstWrite = true;
    return 1;
}
int sio_stop(sio_hdl*) {
    return 1;
}
int sio_flush(sio_hdl*) {
    return 1;
}
int sio_pollfd(sio_hdl*, pollfd*, int) {
    return 0;
}
int sio_revents(sio_hdl*, pollfd*) {
    return 0;
}
int sio_eof(sio_hdl*) {
    return 0;
}
size_t sio_write(sio_hdl*, const void*, size_t bytes) {
    /* sndio reports zero at the first audible sample of each start,
        then frame deltas. preserve that behavior across pause and drain. */
    if (firstWrite) {
        moveCallback(moveContext, 0);
        firstWrite = false;
    }
    moveCallback(moveContext, static_cast<int>(bytes / (negotiated.bps * negotiated.pchan)));
    return bytes;
}
}

int main() {
    SndioOut output(nullptr);
    auto options = mc_output_options MC_OUTPUT_OPTIONS_DEFAULT;
    options.format.sample_rate = 48000;
    options.format.channels = 2;
    options.queue_capacity_frames = 64;
    auto callbacks = mc_output_callbacks MC_OUTPUT_CALLBACKS_DEFAULT;
    callbacks.completed = [](void*, uint64_t, mc_result result) {
        CHECK(result == MC_OK);
        ++returned;
    };
    callbacks.ready = [](void*) {
    };
    std::array<float, 128> samples {};
    auto block = mc_output_buffer MC_OUTPUT_BUFFER_DEFAULT;
    block.token = 1;
    block.pcm.data = samples.data();
    block.pcm.frame_count = 64;
    block.pcm.format = options.format;
    CHECK(output.Prepare(options, callbacks) == MC_OK);
    CHECK(output.QueueAudioBuffer(block) == MC_OK);
    const auto first = timingOf(output);
    CHECK(first.valid_flags & MC_OUTPUT_TIMING_PLAYED);
    CHECK(first.frames_played_by_driver == 64);

    CHECK(output.Pause() == MC_OK);
    CHECK(output.Resume() == MC_OK);
    const auto resumed = timingOf(output);
    CHECK(resumed.generation == first.generation);
    CHECK(resumed.frames_played_by_driver == 64);
    CHECK(output.QueueAudioBuffer(block) == MC_OK);
    CHECK(timingOf(output).frames_played_by_driver == 128);

    CHECK(output.Drain() == MC_OK);
    CHECK(timingOf(output).generation == first.generation);
    CHECK(timingOf(output).frames_played_by_driver == 128);
    CHECK(output.QueueAudioBuffer(block) == MC_OK);
    CHECK(timingOf(output).frames_played_by_driver == 192);
    CHECK(returned == 3);

    CHECK(output.Flush() == MC_OK);
    CHECK(timingOf(output).valid_flags == 0);
    CHECK(output.QueueAudioBuffer(block) == MC_OK);
    const auto flushed = timingOf(output);
    CHECK(flushed.generation != first.generation);
    CHECK(flushed.frames_played_by_driver == 64);
    CHECK(output.Stop() == MC_OK);
    CHECK(timingOf(output).valid_flags == 0);
}
