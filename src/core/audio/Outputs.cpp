//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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
#include "Outputs.h"
#include <core/plugin/PluginFactory.h>
#include <core/support/Preferences.h>
#include <core/support/PreferenceKeys.h>

#include <atomic>
#include <algorithm>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::sdk;
using namespace musik::core::prefs;

using Output = std::shared_ptr<IOutput>;
using OutputList = std::vector<Output>;

#if defined(WIN32)
static const std::string defaultOutput = "WASAPI";
#elif defined(__APPLE__)
static const std::string defaultOutput = "CoreAudio";
#else
static const std::string defaultOutput = "PulseAudio";
#endif

#define LOWER(x) std::transform(x.begin(), x.end(), x.begin(), tolower);

namespace musik {
    namespace core {
        namespace audio {
            namespace outputs {
                Output FindByName(const std::string& name, const OutputList& list) {
                    if (name.size()) {
                        auto it = list.begin();
                        while (it != list.end()) {
                            if ((*it)->Name() == name) {
                                return (*it);
                            }
                            ++it;
                        }
                    }
                    return Output();
                }

                OutputList GetAllOutputs() {
                    using OutputDeleter = PluginFactory::DestroyDeleter<IOutput>;

                    OutputList result = PluginFactory::Instance()
                        .QueryInterface<IOutput, OutputDeleter>("GetAudioOutput");

                    std::sort(
                        result.begin(),
                        result.end(),
                        [](Output left, Output right) -> bool {
                            std::string l = left->Name();
                            LOWER(l);
                            std::string r = right->Name();
                            LOWER(r);
                            return l < r;
                        });

                    return result;
                }

                void SelectOutput(std::shared_ptr<musik::core::sdk::IOutput> output) {
                    std::shared_ptr<Preferences> prefs =
                        Preferences::ForComponent(components::Playback);

                    prefs->SetString(keys::OutputPlugin, output->Name());
                }

                Output SelectedOutput() {
                    std::shared_ptr<Preferences> prefs =
                        Preferences::ForComponent(components::Playback);

                    const OutputList plugins = GetAllOutputs();

                    if (plugins.size()) {
                        /* try to find the user selected plugin first */
                        Output result = FindByName(prefs->GetString(keys::OutputPlugin), plugins);

                        if (!result) {
                            /* fall back to the default */
                            result = FindByName(defaultOutput, plugins);
                        }

                        if (!result) {
                            /* no default? ugh, return the first one. */
                            result = plugins[0];
                        }

                        return result;
                    }

                    return Output();
                }
            }
        }
    }
}