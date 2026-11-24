//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <musikcore/plugin/output.hpp>
#include <AudioToolbox/AudioQueue.h>
#include <array>
#include <atomic>

/* plays float32 PCM through AudioQueue. prepare owns all allocation; callbacks
only release preallocated slots and report completion. Stop quiesces AudioQueue
before returning outstanding tokens. instances cannot be copied or moved. */
class CoreAudioOut {
public:
    CoreAudioOut() = default;
    CoreAudioOut(const CoreAudioOut&) = delete;
    CoreAudioOut(CoreAudioOut&&) = delete;
    ~CoreAudioOut() = default;

    CoreAudioOut& operator=(const CoreAudioOut&) = delete;
    CoreAudioOut& operator=(CoreAudioOut&&) = delete;

    mc_result Prepare(const mc_output_options&, const mc_output_callbacks&);
    mc_result QueueAudioBuffer(const mc_output_buffer&) noexcept;
    mc_result Pause();
    mc_result Resume();
    mc_result SetVolume(double);
    double GetVolume() const;
    mc_result Stop() noexcept;
    mc_result Drain();

    static mc_result VisitDevices(mc_output_device_fn read, void* data);
    static mc_result ReadDefaultDevice(mc_output_device_fn read, void* data);
    mc_result GetDefaultFormat(mc_string_view device, mc_audio_format& format);

private:
    struct Slot {
        AudioQueueBufferRef buffer = nullptr;
        std::atomic<bool> queued { false };
        uint64_t token = 0;
        size_t frames = 0;
    };

    static constexpr size_t BufferCount = 8;
    static void Completed(void*, AudioQueueRef, AudioQueueBufferRef) noexcept;
    static void RunningChanged(void*, AudioQueueRef, AudioQueuePropertyID) noexcept;
    void NotifyDrainWaiter() noexcept;
    void FinishProducer() noexcept;

    AudioQueueRef queue = nullptr;
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    std::array<Slot, BufferCount> slots;
    size_t capacity = 0;
    std::atomic<size_t> queuedFrames { 0 };
    std::atomic<unsigned> producers { 0 };
    std::atomic<bool> stopped { true };
    std::atomic<bool> paused { false };
    bool restartAfterDrain = false;
    std::atomic<double> volume { 1.0 };
    std::atomic<uint64_t> drainNotifications { 0 };
};
