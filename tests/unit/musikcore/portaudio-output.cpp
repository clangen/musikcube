#include <plugins/portaudioout/PortAudioOut.h>

#include <cstdio>
#include <cstdlib>
#include <latch>
#include <thread>

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::fprintf(stderr, "PortAudio test failed at line %d: %s\n", __LINE__, #condition);                      \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (false)

namespace {
    void* streamContext = nullptr;
    PaStreamFinishedCallback* finishedCallback = nullptr;
    PaStreamCallback* streamCallback = nullptr;
    int fakeStream = 0;
    PaDeviceInfo device {};

    template<class T> mc_output_timing timingOf(const T& output) {
        auto timing = mc_output_timing MC_OUTPUT_TIMING_DEFAULT;
        if constexpr (musik::core::plugin::detail::HasOutputTiming<T>::value) {
            CHECK(output.GetTiming(timing) == MC_OK);
        }
        return timing;
    }

    struct Callbacks {
        unsigned ready = 0;
        unsigned completed = 0;
        mc_result result = MC_OK;
    };

    void MC_CALL complete(void* context, uint64_t, mc_result result) {
        auto& callbacks = *static_cast<Callbacks*>(context);
        ++callbacks.completed;
        callbacks.result = result;
    }

    void MC_CALL ready(void* context) {
        ++static_cast<Callbacks*>(context)->ready;
    }
}

extern "C" {
PaError Pa_Initialize() {
    return paNoError;
}
PaError Pa_Terminate() {
    return paNoError;
}
PaDeviceIndex Pa_GetDeviceCount() {
    return 1;
}
PaDeviceIndex Pa_GetDefaultOutputDevice() {
    return 0;
}
const PaDeviceInfo* Pa_GetDeviceInfo(PaDeviceIndex) {
    return &device;
}
PaError Pa_OpenStream(PaStream** stream, const PaStreamParameters*, const PaStreamParameters*, double, unsigned long,
    PaStreamFlags, PaStreamCallback* callback, void* context) {
    streamContext = context;
    streamCallback = callback;
    *stream = &fakeStream;
    return paNoError;
}
PaError Pa_SetStreamFinishedCallback(PaStream*, PaStreamFinishedCallback* callback) {
    finishedCallback = callback;
    return paNoError;
}
const PaStreamInfo* Pa_GetStreamInfo(PaStream*) {
    return nullptr;
}
PaError Pa_StartStream(PaStream*) {
    return paNoError;
}
PaError Pa_StopStream(PaStream*) {
    return paNoError;
}
PaError Pa_AbortStream(PaStream*) {
    finishedCallback(streamContext);
    return paNoError;
}
PaError Pa_CloseStream(PaStream*) {
    return paNoError;
}
}

int main() {
    device.name = "test";
    device.maxOutputChannels = 2;
    device.defaultSampleRate = 48000;

    PortAudioOut output;
    Callbacks observations;
    auto callbacks = mc_output_callbacks MC_OUTPUT_CALLBACKS_DEFAULT;
    callbacks.context = &observations;
    callbacks.completed = complete;
    callbacks.ready = ready;
    auto options = mc_output_options MC_OUTPUT_OPTIONS_DEFAULT;
    options.format.sample_rate = 48000;
    options.format.channels = 2;
    options.queue_capacity_frames = 1;
    CHECK(output.Prepare(options, callbacks) == MC_OK);

    float samples[2] {};
    auto buffer = mc_output_buffer MC_OUTPUT_BUFFER_DEFAULT;
    buffer.token = 1;
    buffer.pcm.data = samples;
    buffer.pcm.frame_count = 1;
    buffer.pcm.format = options.format;
    CHECK(output.QueueAudioBuffer(buffer) == MC_OK);
    CHECK(output.QueueAudioBuffer(buffer) == MC_ERROR_QUEUE_FULL);

    /* queued storage is not audible yet. the callback reports 100 ms until
    its first sample reaches the DAC, even though it returns the token now. */
    CHECK(timingOf(output).valid_flags & MC_OUTPUT_TIMING_QUEUED);
    float rendered[2] {};
    PaStreamCallbackTimeInfo clock { 0.0, 10.0, 10.1 };
    CHECK(streamCallback(nullptr, rendered, 1, &clock, 0, streamContext) == paContinue);
    const auto scheduled = timingOf(output);
    CHECK(scheduled.valid_flags & MC_OUTPUT_TIMING_PLAYED);
    CHECK(scheduled.valid_flags & MC_OUTPUT_TIMING_ESTIMATED);
    CHECK(scheduled.frames_played_by_driver == 0);
    CHECK(scheduled.queued_seconds > 0);
    CHECK(!(scheduled.valid_flags & MC_OUTPUT_TIMING_DEVICE_LATENCY));
    CHECK(observations.completed == 1);
    CHECK(output.Stop() == MC_OK);
    observations = {};
    CHECK(output.Prepare(options, callbacks) == MC_OK);
    CHECK(timingOf(output).generation != scheduled.generation);
    CHECK(output.QueueAudioBuffer(buffer) == MC_OK);

    /* a lost device must wake a producer and reject its retry. */
    finishedCallback(streamContext);
    CHECK(observations.ready == 1);
    CHECK(observations.completed == 1);
    CHECK(observations.result == MC_ERROR_IO);
    CHECK(output.QueueAudioBuffer(buffer) == MC_ERROR_IO);
    CHECK(output.Drain() == MC_ERROR_IO);
    CHECK(output.Stop() == MC_OK);
    CHECK(observations.completed == 1);
    CHECK(observations.result == MC_ERROR_IO);

    /* a deliberate flush also triggers PortAudio's finished callback, but the
    restarted device must continue accepting audio. */
    observations = {};
    CHECK(output.Prepare(options, callbacks) == MC_OK);
    CHECK(output.Flush() == MC_OK);
    CHECK(observations.ready == 0);
    CHECK(output.QueueAudioBuffer(buffer) == MC_OK);
    CHECK(output.Stop() == MC_OK);
    CHECK(observations.completed == 1);

    /* `Stop()` racing a producer must return every accepted token once. */
    for (int iteration = 0; iteration < 300; ++iteration) {
        observations = {};
        CHECK(output.Prepare(options, callbacks) == MC_OK);
        std::latch start(1);
        mc_result submitted = MC_ERROR_INTERNAL;
        std::thread producer([&] {
            start.wait();
            submitted = output.QueueAudioBuffer(buffer);
        });
        start.count_down();
        CHECK(output.Stop() == MC_OK);
        producer.join();
        CHECK(submitted == MC_OK || submitted == MC_ERROR_CANCELLED);
        CHECK(observations.completed == (submitted == MC_OK ? 1u : 0u));
        if (submitted == MC_OK) {
            CHECK(observations.result == MC_ERROR_CANCELLED);
        }
    }

    /* device loss can overlap an accepted write; the token must still return. */
    for (int iteration = 0; iteration < 300; ++iteration) {
        observations = {};
        CHECK(output.Prepare(options, callbacks) == MC_OK);
        std::latch start(1);
        mc_result submitted = MC_ERROR_INTERNAL;
        std::thread producer([&] {
            start.wait();
            submitted = output.QueueAudioBuffer(buffer);
        });
        start.count_down();
        finishedCallback(streamContext);
        producer.join();
        CHECK(submitted == MC_OK || submitted == MC_ERROR_IO);
        CHECK(observations.completed == (submitted == MC_OK ? 1u : 0u));
        if (submitted == MC_OK) {
            CHECK(observations.result == MC_ERROR_IO);
        }
        CHECK(output.Stop() == MC_OK);
    }
}
