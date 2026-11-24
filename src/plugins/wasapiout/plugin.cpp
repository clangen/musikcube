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

#include "WasapiOutput.h"
#include "WasapiSettings.h"
#include <musikcore/plugin/registration.hpp>

namespace {
    constexpr mc_boolean_setting booleans[] { wasapi::EndpointRouting, wasapi::DecoderResampling };
    const mc_configuration_api configuration {
        .struct_size = sizeof(mc_configuration_api),
        .integers = nullptr,
        .integer_count = 0,
        .booleans = booleans,
        .boolean_count = 2,
        .numbers = nullptr,
        .number_count = 0,
        .texts = nullptr,
        .text_count = 0,
    };

    mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        const auto result = musik::core::plugin::RegisterOutput<WasapiOutput>(*host);
        return result == MC_OK ? musik::core::plugin::RegisterConfiguration(*host, configuration) : result;
    }

    void MC_CALL shutdown(void*) {}
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "871cb3c2-0002-49cd-9410-5207cb3cfd4a", 36 };
    plugin->name = { "WASAPI", 6 };
    plugin->version = { "1", 1 };
    plugin->author = { "clangen", 7 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
