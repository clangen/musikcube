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

#include "pch.hpp"

#include "Dsp.h"
#include <musikcore/plugin/Plugins.h>
#include <musikcore/audio/Buffer.h>
#include <common/Contract.h>

using namespace musik::core::audio;
using namespace musik::core::sdk;

namespace musik { namespace core { namespace audio {

    Dsp::Dsp(musik::core::ffi::PluginRegistry::Factory<mc_dsp_factory> factory, const mc_plugin_services& services)
    : factory(std::move(factory)) {
        this->factory.Invoke([&](const mc_dsp_factory& table) {
            const auto status = table.create(table.context, &services, &this->instance, nullptr);
            musik::contract::Assert(status == MC_OK || !this->instance,
                "failed DSP factory returned an instance");
            if (status == MC_OK && this->instance) {
                this->api = table.instance_api;
            }
            return status;
        });
    }

    Dsp::~Dsp() {
        if (this->instance) {
            this->api->destroy(this->instance);
        }
    }

    mc_result Dsp::Process(Buffer& buffer) {
        const auto channels = static_cast<uint32_t>(buffer.Channels());
        if (!channels || buffer.Samples() < 0 || buffer.Samples() % channels != 0 ||
            buffer.SampleRate() <= 0 || !buffer.BufferPointer())
        {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        const auto frames = static_cast<size_t>(buffer.Samples() / channels);
        mc_audio_buffer_view view {
            .struct_size = sizeof(mc_audio_buffer_view),
            .data = buffer.BufferPointer(),
            .frame_count = frames,
            .frame_capacity = frames,
            .format = {
                .struct_size = sizeof(mc_audio_format),
                .sample_rate = static_cast<uint32_t>(buffer.SampleRate()),
                .channels = channels,
                .channel_layout = 0,
            },
        };
        return this->factory.Invoke([&](const mc_dsp_factory&) {
            return this->api->process(this->instance, &view);
        });
    }

} } }
