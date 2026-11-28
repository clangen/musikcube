#pragma once

#include <musikcore/plugin/output.hpp>
#include <sndio.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <poll.h>
#include <vector>

/* sndio copies PCM during nonblocking writes. one producer owns the write
operation; a pipe wakes its poll when Stop() interrupts it. */
class SndioOut {
public:
    explicit SndioOut(const mc_plugin_services* services) : services(services) {}
    ~SndioOut() noexcept { Stop(); }

    mc_result Prepare(const mc_output_options&, const mc_output_callbacks&);
    mc_result QueueAudioBuffer(const mc_output_buffer&) noexcept;
    mc_result Pause();
    mc_result Resume();
    mc_result SetVolume(double);
    double GetVolume() const;
    mc_result Stop() noexcept;
    mc_result Drain();
    mc_result Flush();
    mc_result GetTiming(mc_output_timing&) const noexcept;
    mc_result GetDefaultFormat(mc_string_view, mc_audio_format&);
    static mc_result VisitDevices(mc_output_device_fn, void*);
    static mc_result ReadDefaultDevice(mc_output_device_fn, void*);

private:
    static void OnMove(void*, int) noexcept;
    mc_result WaitForDevice();
    void ResetTiming() noexcept;

    mutable std::mutex mutex;
    const mc_plugin_services* services;
    std::condition_variable operationEnded;
    sio_hdl* handle = nullptr;
    /* interrupts poll before Stop() waits for the borrowed buffer. */
    int wake[2] { -1, -1 };
    std::vector<pollfd> descriptors;
    std::vector<int16_t> samples;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    size_t capacity = 0;
    double volume = 1.0;
    bool stopped = true;
    bool started = false;
    bool paused = false;
    bool operationActive = false;
    /* `sio_onmove()` updates the last device-clock observation. */
    std::atomic<uint32_t> timingVersion { 0 };
    std::atomic<uint64_t> generation { 0 };
    std::atomic<uint64_t> played { 0 };
    std::atomic<uint64_t> playedTime { 0 };
};
