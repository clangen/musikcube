//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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
#elif defined(__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__) || defined (__DragonFly__)
static const std::string defaultOutput = "sndio";
#else
static const std::string defaultOutput = "PulseAudio";
#endif

#define LOWER(x) std::transform(x.begin(), x.end(), x.begin(), tolower);

class NoOutput: public IOutput {
    public:
        virtual void Release() { delete this; }
        virtual void Pause() { }
        virtual void Resume() { }
        virtual void SetVolume(double volume) { this->volume = volume; }
        virtual double GetVolume() { return this->volume; }
        virtual void Stop() { }
        virtual int Play(IBuffer *buffer, IBufferProvider *provider) { return OutputInvalidState; }
        virtual void Drain() { }
        virtual double Latency() { return 0.0; }
        virtual const char* Name() { return "NoOutput"; }
        virtual IDeviceList* GetDeviceList() { return nullptr; }
        virtual bool SetDefaultDevice(const char* deviceId) { return false; }
        virtual IDevice* GetDefaultDevice() { return nullptr; }
    private:
        double volume{ 1.0f };
};

namespace musik {
    namespace core {
        namespace audio {
            namespace outputs {
                using ReleaseDeleter = PluginFactory::ReleaseDeleter<IOutput>;
                using NullDeleter = PluginFactory::NullDeleter<IOutput>;

                static inline void release(OutputList outputs) {
                    for (auto output : outputs) {
                        output->Release();
                    }
                }

                template <typename D>
                OutputList queryOutputs() {
                    OutputList result = PluginFactory::Instance()
                        .QueryInterface<IOutput, D>("GetAudioOutput");

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

                static Output findByName(const std::string& name, const OutputList& list) {
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
                    return queryOutputs<ReleaseDeleter>();
                }

                void SelectOutput(std::shared_ptr<musik::core::sdk::IOutput> output) {
                    SelectOutput(output.get());
                }

                void SelectOutput(musik::core::sdk::IOutput* output) {
                    if (output) {
                        std::shared_ptr<Preferences> prefs =
                            Preferences::ForComponent(components::Playback);

                        prefs->SetString(keys::OutputPlugin, output->Name());
                    }
                }

                size_t GetOutputCount() {
                    return queryOutputs<ReleaseDeleter>().size();
                }

                musik::core::sdk::IOutput* GetUnmanagedOutput(size_t index) {
                    auto all = queryOutputs<NullDeleter>();
                    if (!all.size()) {
                        return new NoOutput();
                    }
                    auto output = all[index].get();
                    all.erase(all.begin() + index);
                    release(all);
                    return output;
                }

                musik::core::sdk::IOutput* GetUnmanagedOutput(const std::string& name) {
                    auto all = queryOutputs<NullDeleter>();
                    IOutput* output = nullptr;
                    for (size_t i = 0; i < all.size(); i++) {
                        if (all[i]->Name() == name) {
                            output = all[i].get();
                            all.erase(all.begin() + i);
                            break;
                        }
                    }
                    release(all);
                    return output == nullptr ? new NoOutput() : output;
                }

                musik::core::sdk::IOutput* GetUnmanagedSelectedOutput() {
                    IOutput* output = nullptr;

                    OutputList plugins = queryOutputs<NullDeleter>();

                    if (plugins.size()) {
                        std::shared_ptr<Preferences> prefs =
                            Preferences::ForComponent(components::Playback);

                        const std::string name = prefs->GetString(keys::OutputPlugin);

                        for (size_t i = 0; i < plugins.size(); i++) {
                            if (plugins[i]->Name() == name) {
                                output = plugins[i].get();
                                plugins.erase(plugins.begin() + i);
                                break;
                            }
                        }

                        if (!output) {
                            output = plugins[0].get();
                            plugins.erase(plugins.begin());
                        }
                    }

                    release(plugins);

                    return output == nullptr ? new NoOutput() : output;
                }

                Output SelectedOutput() {
                    std::shared_ptr<Preferences> prefs =
                        Preferences::ForComponent(components::Playback);

                    const OutputList plugins = queryOutputs<ReleaseDeleter>();

                    if (plugins.size()) {
                        /* try to find the user selected plugin first */
                        Output result = findByName(prefs->GetString(keys::OutputPlugin), plugins);

                        if (!result) {
                            /* fall back to the default */
                            result = findByName(defaultOutput, plugins);
                        }

                        if (!result) {
                            /* no default? ugh, return the first one. */
                            result = plugins[0];
                        }

                        return result;
                    }

                    return Output(new NoOutput());
                }
            }
        }
    }
}
