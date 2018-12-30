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
#include "Visualizer.h"
#include <core/plugin/PluginFactory.h>

#include <atomic>
#include <algorithm>

using namespace musik::core::audio;
using namespace musik::core::sdk;

static std::vector<std::shared_ptr<IVisualizer > > visualizers;
static std::atomic<bool> initialized;

static std::shared_ptr<IVisualizer> selectedVisualizer;
static ISpectrumVisualizer* spectrumVisualizer = nullptr;
static IPcmVisualizer* pcmVisualizer = nullptr;

#define LOWER(x) std::transform(x.begin(), x.end(), x.begin(), tolower);

namespace musik {
    namespace core {
        namespace audio {
            namespace vis {
                static void init() {
                    /* spectrum visualizers */
                    typedef PluginFactory::ReleaseDeleter<ISpectrumVisualizer> SpectrumDeleter;
                    std::vector<std::shared_ptr<ISpectrumVisualizer> > spectrum;

                    spectrum = PluginFactory::Instance()
                        .QueryInterface<ISpectrumVisualizer, SpectrumDeleter>("GetSpectrumVisualizer");

                    for (auto it = spectrum.begin(); it != spectrum.end(); it++) {
                        visualizers.push_back(*it);
                    }

                    /* pcm visualizers */
                    typedef PluginFactory::ReleaseDeleter<IPcmVisualizer> PcmDeleter;
                    std::vector<std::shared_ptr<IPcmVisualizer> > pcm;

                    pcm = PluginFactory::Instance()
                        .QueryInterface<IPcmVisualizer, PcmDeleter>("GetPcmVisualizer");

                    for (auto it = pcm.begin(); it != pcm.end(); it++) {
                        visualizers.push_back(*it);
                    }

                    /* sort 'em by name */
                    using V = std::shared_ptr<IVisualizer>;

                    std::sort(
                        visualizers.begin(),
                        visualizers.end(),
                        [](V left, V right) -> bool {
                            std::string l = left->Name();
                            LOWER(l);
                            std::string r = right->Name();
                            LOWER(r);
                            return l < r;
                        });

                    initialized = true;
                }

                void HideSelectedVisualizer() {
                    if (selectedVisualizer) {
                        selectedVisualizer->Hide();
                        SetSelectedVisualizer(std::shared_ptr<IVisualizer>());
                    }
                }

                ISpectrumVisualizer* SpectrumVisualizer() {
                    return spectrumVisualizer;
                }

                IPcmVisualizer* PcmVisualizer() {
                    return pcmVisualizer;
                }

                std::shared_ptr<IVisualizer> GetVisualizer(size_t index) {
                    return visualizers.at(index);
                }

                size_t VisualizerCount() {
                    if (!initialized) {
                        init();
                    }

                    return visualizers.size();
                }

                void SetSelectedVisualizer(std::shared_ptr<IVisualizer> visualizer) {
                    selectedVisualizer = visualizer;
                    pcmVisualizer = dynamic_cast<IPcmVisualizer*>(visualizer.get());
                    spectrumVisualizer = dynamic_cast<ISpectrumVisualizer*>(visualizer.get());
                }

                std::shared_ptr<IVisualizer> SelectedVisualizer() {
                    return selectedVisualizer;
                }
            }
        }
    }
}