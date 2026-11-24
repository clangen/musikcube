//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#include <common/FixedRingQueue.h>
#include <musikcore/plugin/output.hpp>
#include <musikcore/plugin/services.h>
#include <pipewire/pipewire.h>
#include <atomic>
#include <condition_variable>
#include <mutex>

/* PipeWire's non-RT loop owns the bounded input ring. control calls take the
loop lock; stop wakes a drain before joining and destroying the loop. */
class PipeWireOut {
public:
    explicit PipeWireOut(const mc_plugin_services* services) : services(services) {}
    PipeWireOut(const PipeWireOut&) = delete;
    PipeWireOut(PipeWireOut&&) = delete;
    ~PipeWireOut() = default;
    PipeWireOut& operator=(const PipeWireOut&) = delete;
    PipeWireOut& operator=(PipeWireOut&&) = delete;

    mc_result Prepare(const mc_output_options&, const mc_output_callbacks&);
    mc_result QueueAudioBuffer(const mc_output_buffer&) noexcept;
    mc_result Pause();
    mc_result Resume();
    mc_result SetVolume(double);
    double GetVolume() const;
    mc_result Stop() noexcept;
    mc_result Drain();
    mc_result Flush();

    static mc_result VisitDevices(mc_output_device_fn, void*);
    static mc_result ReadDefaultDevice(mc_output_device_fn, void*);
    mc_result GetDefaultFormat(mc_string_view, mc_audio_format&);

private:
    /* a PCM block from musikcore waiting to be copied into PipeWire buffers.
    `framesCopied` tracks progress when one block spans multiple buffers. */
    struct QueuedBuffer {
        mc_output_buffer buffer = MC_OUTPUT_BUFFER_DEFAULT;
        size_t framesCopied = 0;
    };

    static constexpr size_t MaxQueuedBuffers = 64;
    static void StateChanged(void*, pw_stream_state, pw_stream_state, const char*) noexcept;
    static void Process(void*) noexcept;
    static void Drained(void*) noexcept;
    void ReturnPending(mc_result) noexcept;

    const mc_plugin_services* services;
    std::mutex lifecycle;
    std::condition_variable finished;
    pw_thread_loop* loop = nullptr;
    pw_stream* stream = nullptr;
    pw_stream_events events {};
    bool loopStarted = false;
    bool stopped = true;
    bool paused = false;
    bool failed = false;
    bool draining = false;
    bool drained = false;
    unsigned activeDrains = 0;
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;

    /* a circular queue protected by the PipeWire loop lock. musikcore keeps
    each block's samples alive until its token is returned through the callback. */
    musik::FixedRingQueue<QueuedBuffer, MaxQueuedBuffers> queuedBuffers;
    size_t maxQueuedBuffers = 16;
    size_t queuedFrames = 0;
    size_t capacity = 0;
    std::atomic<float> volume { 1.0f };
};
