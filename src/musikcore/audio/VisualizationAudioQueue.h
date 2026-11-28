#pragma once

#include <common/DeleteDefaults.h>
#include <musikcore/plugin/audio.h>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>
#include <thread>

namespace musik { namespace core { namespace audio {

    /* owns PCM waiting to be sent to visualizer plugins and the spectrum meter.
    Output pumps each accepted audio block into `Enqueue()`. this queue copies
    only when a consumer is active, so decoder buffers can be recycled freely.

    one worker asks AudiblePlaybackPosition for the current audible output-frame index,
    then delivers the latest audible window through `vis::DeliverAudio()`. timing flags,
    device latency, and track media positions are interpreted elsewhere.

    the worker starts on first capture and sleeps while empty or paused. memory
    is bounded, stale windows are skipped, and capture resource errors
    skip visualization with one diagnostic per reset rather than fail playback. */
    class VisualizationAudioQueue {
    public:
        MUSIK_DELETE_COPY_AND_MOVE(VisualizationAudioQueue);

        /* Output supplies an audible-frame reader and outlives this queue.
        an unavailable reading means keep waiting. the callback runs on this
        queue's worker without its mutex; it may lock the owning output. */
        explicit VisualizationAudioQueue(std::function<std::optional<uint64_t>()> readAudibleFrame);

        /* stop and join the worker before the output or its callback target
        is destroyed. pending visual frames are discarded. */
        ~VisualizationAudioQueue();

        /* capture an accepted block on the producer worker, never an audio
        callback. firstFrame locates its first PCM frame in AudiblePlaybackPosition.
        the borrowed view has a valid format and complete frames; nothing from
        it is retained after return. inactive consumers and oversized blocks
        are skipped. allocation/thread failures skip capture and are reported
        internally; they never turn accepted audio into a failed queue result. */
        void Enqueue(const mc_pcm_view& pcm, uint64_t firstFrame) noexcept;

        /* discard pending PCM on seek, stop, or output reconfiguration, and
        release the paused state. waits for an in-progress delivery to finish,
        so none of the discarded audio can be delivered after return. call
        before changing the clock's frame numbering or sample format. */
        void Reset();

        /* suspend visual delivery while playback is paused, retaining queued
        PCM for resume. this does not pause the device or change its clock;
        Output handles those operations separately. */
        void SetPaused(bool paused);

    private:
        /* owns timestamped audio until the audible cursor passes it. create
        a borrowed view of the latest audible window only when delivering. */
        struct PendingBuffer {
            std::vector<float> samples;
            mc_audio_format format;
            uint64_t firstFrame;
        };

        mc_result Capture(const mc_pcm_view& pcm, uint64_t firstFrame) noexcept;
        void Run();

        const std::function<std::optional<uint64_t>()> readAudibleFrame;
        std::mutex mutex;
        std::condition_variable changed;
        std::deque<PendingBuffer> pendingBuffers;
        size_t queuedSamples = 0;
        uint64_t epoch = 0;
        std::optional<uint64_t> deliveredThrough;
        bool captureWarning = false;
        bool paused = false;
        bool quit = false;
        std::thread worker;
    };

} } }
