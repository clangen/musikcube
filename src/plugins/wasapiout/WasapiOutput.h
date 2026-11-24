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

#include <musikcore/plugin/output.hpp>
#include <memory>

/* one worker owns the COM apartment and all WASAPI interfaces. */
class WasapiOutput {
public:
    explicit WasapiOutput(const mc_plugin_services* services);
    ~WasapiOutput() noexcept;
    WasapiOutput(const WasapiOutput&) = delete;
    WasapiOutput& operator=(const WasapiOutput&) = delete;

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

    static mc_result VisitDevices(mc_output_device_fn, void*);
    static mc_result ReadDefaultDevice(mc_output_device_fn, void*);
    mc_result GetDefaultFormat(mc_string_view, mc_audio_format&);

private:
    struct State;
    std::unique_ptr<State> state;
    const mc_plugin_services* services;
};
