/* plays host-owned float samples through PortAudio. a bounded single-producer
queue keeps allocation and locks out of the hardware callback. */
#pragma once

#include <musikcore/plugin/output.hpp>
#include <plugins/common/OutputTimingEstimator.h>
#include <portaudio.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

class PortAudioOut {
public:
    PortAudioOut();
    ~PortAudioOut();

    PortAudioOut(const PortAudioOut&) = delete;
    PortAudioOut& operator=(const PortAudioOut&) = delete;

    mc_result Prepare(const mc_output_options&, const mc_output_callbacks&);
    mc_result QueueAudioBuffer(const mc_output_buffer&) noexcept;
    mc_result Pause();
    mc_result Resume();
    mc_result SetVolume(double);
    double GetVolume() const;
    mc_result Stop() noexcept;
    mc_result Drain();
    mc_result Flush();
    mc_result GetTiming(mc_output_timing& timing) const noexcept { return this->timingEstimator.Read(timing); }
    mc_result GetDefaultFormat(mc_string_view, mc_audio_format&);

    static mc_result VisitDevices(mc_output_device_fn, void*);
    static mc_result ReadDefaultDevice(mc_output_device_fn, void*);

private:
    static constexpr size_t SlotCount = 32;
    static constexpr size_t DefaultCapacity = 32768;
    /* reserve the top bit for device loss; the remaining bits count writes. */
    static constexpr unsigned FailureFlag = 1u << 31;

    struct Slot {
        const float* samples = nullptr;
        size_t frames = 0;
        uint64_t token = 0;
    };

    static int Callback(
        const void*, void*, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*) noexcept;
    static void Finished(void*) noexcept;
    mc_result StartStream();
    void ReturnPending(mc_result) noexcept;
    void ReturnFailedBuffers() noexcept;
    void Wake() noexcept;
    void FinishProducer() noexcept;

    std::array<Slot, SlotCount> slots {};
    std::atomic<size_t> head { 0 };
    std::atomic<size_t> tail { 0 };
    std::atomic<size_t> queuedFrames { 0 };
    size_t offset = 0;
    size_t capacity = DefaultCapacity;
    musik::plugins::OutputTimingEstimator timingEstimator;
    uint64_t scheduledFrames = 0;
    double outputLatency = 0.0;
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    std::mutex streamMutex;
    PaStream* stream = nullptr;
    bool initialized = false;
    std::atomic<bool> stopped { true };
    std::atomic<bool> paused { false };
    std::atomic<bool> draining { false };
    std::atomic<bool> finished { false };
    std::atomic<bool> requestedCompletion { false };
    std::atomic<bool> aborting { false };
    /* the device callback or the last producer returns failed buffers, once. */
    std::atomic<bool> failureClaimed { false };
    std::atomic<unsigned> producers { 0 };
    std::atomic<unsigned> wakeGeneration { 0 };
    std::atomic<float> volume { 1.0f };
};
