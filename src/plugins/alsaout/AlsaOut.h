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
#include <alsa/asoundlib.h>
#include <condition_variable>
#include <mutex>
#include <poll.h>
#include <vector>

/* owns one nonblocking ALSA handle; a wake descriptor interrupts producer
and drain waits before `Stop()` closes the handle. */
class AlsaOut {
public:
    explicit AlsaOut(const mc_plugin_services* services) : services(services) {}
    AlsaOut(const AlsaOut&) = delete;
    AlsaOut(AlsaOut&&) = delete;
    ~AlsaOut() = default;
    AlsaOut& operator=(const AlsaOut&) = delete;
    AlsaOut& operator=(AlsaOut&&) = delete;

    mc_result Prepare(const mc_output_options&, const mc_output_callbacks&);
    mc_result QueueAudioBuffer(const mc_output_buffer&) noexcept;
    mc_result Pause();
    mc_result Resume();
    mc_result SetVolume(double);
    double GetVolume() const;
    mc_result Stop() noexcept;
    mc_result Drain();

    static mc_result VisitDevices(mc_output_device_fn, void*);
    static mc_result ReadDefaultDevice(mc_output_device_fn, void*);
    static mc_result GetDefaultFormat(mc_string_view, mc_audio_format&);

private:
    mc_result WaitForDevice();
    void EndOperation() noexcept;

    mutable std::mutex mutex;
    std::condition_variable operationEndedEvent;
    const mc_plugin_services* services = nullptr;
    snd_pcm_t* pcm = nullptr;

    /* `Stop()` writes to this eventfd to wake a thread waiting for the device.
    -1 means the eventfd has not been opened or has already been closed. */
    int wake = -1;

    /* `poll()` waits on these descriptors until ALSA is ready or `Stop()` wakes it.
    ALSA supplies the device descriptors; the last entry is `wake`. */
    std::vector<pollfd> descriptors;

    std::vector<float> scaled;
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    size_t capacity = 0;
    double volume = 1.0;
    bool stopped = true;
    bool paused = false;
    bool operationActive = false;
};
