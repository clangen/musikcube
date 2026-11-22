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
#include "Visualizer.h"

#include <musikcore/ffi/common/error.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/visualizer.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <string_view>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::sdk;

namespace {
    /* Keep registry entries alive while playback workers use borrowed pointers. */
    std::vector<std::shared_ptr<IVisualizer>> visualizers;
    std::atomic<IVisualizer*> selectedVisualizer { nullptr };
    bool initialized = false;
    std::atomic<bool> spectrumEnabled { false };
    std::atomic<uint64_t> spectrumVersion { 0 };
    uint64_t enabledVersion = 0;
    std::atomic<uint32_t> spectrumSampleRate { 0 };
    std::array<std::atomic<float>, MC_VISUALIZER_SPECTRUM_BINS> spectrum {};
    static_assert(std::atomic<float>::is_always_lock_free);

    std::string normalizedName(IVisualizer& visualizer) {
        std::string name = visualizer.Name();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return std::tolower(c);
        });
        return name;
    }

    void initializeVisualizers() {
        if (initialized) {
            return;
        }

        auto& factory = PluginFactory::Instance();
        auto spectrum = factory.QueryInterface<ISpectrumVisualizer,
            PluginFactory::ReleaseDeleter<ISpectrumVisualizer>>("GetSpectrumVisualizer");
        auto pcm = factory.QueryInterface<IPcmVisualizer,
            PluginFactory::ReleaseDeleter<IPcmVisualizer>>("GetPcmVisualizer");

        std::vector<std::shared_ptr<IVisualizer>> discovered(spectrum.begin(), spectrum.end());
        discovered.insert(discovered.end(), pcm.begin(), pcm.end());
        std::sort(discovered.begin(), discovered.end(), [](const auto& left, const auto& right) {
            return normalizedName(*left) < normalizedName(*right);
        });
        visualizers = std::move(discovered);
        initialized = true;
    }
}

IVisualizer* vis::SelectedVisualizer() {
    return selectedVisualizer.load();
}

bool vis::SpectrumEnabled() {
    return spectrumEnabled.load(std::memory_order_relaxed);
}

void vis::PublishSpectrum(const float* bins, uint32_t sampleRate) {
    auto version = spectrumVersion.load(std::memory_order_relaxed);
    /* Concurrent crossfade workers skip a frame when another writer is active. */
    if ((version & 1) || !spectrumVersion.compare_exchange_strong(version, version + 1,
        std::memory_order_acq_rel))
    {
        return;
    }
    spectrumSampleRate.store(sampleRate, std::memory_order_relaxed);
    for (size_t i = 0; i < spectrum.size(); ++i) {
        spectrum[i].store(bins[i], std::memory_order_relaxed);
    }
    spectrumVersion.store(version + 2, std::memory_order_release);
}

mc_result MC_CALL mc_visualizer_set_spectrum_enabled(bool enabled) {
    if (enabled && !vis::SpectrumEnabled()) {
        enabledVersion = spectrumVersion.load(std::memory_order_acquire);
    }
    spectrumEnabled.store(enabled, std::memory_order_relaxed);
    return MC_OK;
}

mc_result MC_CALL mc_visualizer_read_spectrum(float* bins, size_t capacity, uint32_t* sampleRate) {
    if (!bins || capacity < spectrum.size() || !sampleRate) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    const auto version = spectrumVersion.load(std::memory_order_acquire);
    if (!vis::SpectrumEnabled() || version == enabledVersion) {
        return MC_ERROR_NOT_FOUND;
    }
    if (version & 1) {
        return MC_ERROR_BUSY;
    }

    std::array<float, MC_VISUALIZER_SPECTRUM_BINS> snapshot;
    const auto rate = spectrumSampleRate.load(std::memory_order_relaxed);
    for (size_t i = 0; i < spectrum.size(); ++i) {
        snapshot[i] = spectrum[i].load(std::memory_order_relaxed);
    }
    std::atomic_thread_fence(std::memory_order_acquire);
    if (spectrumVersion.load(std::memory_order_relaxed) != version) {
        return MC_ERROR_BUSY;
    }
    std::copy(snapshot.begin(), snapshot.end(), bins);
    *sampleRate = rate;
    return MC_OK;
}

void vis::Shutdown() {
    spectrumEnabled.store(false, std::memory_order_relaxed);
    if (auto* selected = selectedVisualizer.exchange(nullptr)) {
        selected->Hide();
    }
    /* worker users have stopped; the next session rediscovers enabled providers. */
    visualizers.clear();
    initialized = false;
}

mc_result MC_CALL mc_visualizer_list(mc_visualizer_list_fn read, void* data) {
    if (!read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return ffi::exception_barrier(nullptr, [&] {
        initializeVisualizers();
        std::vector<mc_string_view> names;
        names.reserve(visualizers.size());
        for (const auto& visualizer : visualizers) {
            const std::string_view name(visualizer->Name());
            names.push_back({ name.data(), name.size() });
        }
        read(data, names.data(), names.size());
        return MC_OK;
    });
}

mc_result MC_CALL mc_visualizer_show(size_t index) {
    return ffi::exception_barrier(nullptr, [&] {
        initializeVisualizers();
        if (index >= visualizers.size()) {
            return MC_ERROR_NOT_FOUND;
        }

        auto* visualizer = visualizers[index].get();
        auto* previous = selectedVisualizer.exchange(nullptr);
        if (previous && previous != visualizer && previous->Visible()) {
            previous->Hide();
        }
        visualizer->Show();
        selectedVisualizer.store(visualizer);
        return MC_OK;
    });
}

mc_result MC_CALL mc_visualizer_hide() {
    return ffi::exception_barrier(nullptr, [&] {
        auto* selected = selectedVisualizer.exchange(nullptr);
        if (!selected || !selected->Visible()) {
            return MC_ERROR_NOT_FOUND;
        }

        selected->Hide();
        return MC_OK;
    });
}

mc_result MC_CALL mc_visualizer_is_visible(bool* outVisible) {
    if (!outVisible) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        auto* selected = selectedVisualizer.load();
        *outVisible = selected && selected->Visible();
        return MC_OK;
    });
}
