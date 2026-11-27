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
#include <stdexcept>
#include "Plugins.h"
#include "PluginMediaService.h"
#include "PlaybackRemotes.h"
#include <musikcore/ffi/plugin_abi/AudioScratchBuffers.h>
#include <musikcore/equalizer.h>
#include "../ffi/common/error.h"
#include "../ffi/common/primitives.h"
#include <musikcore/ffi/consumer/settings-shape.h>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <thread>
#include <common/Filesystem.h>
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>

#include <musikcore/debug.h>
#include <common/Contract.h>
#include <common/version.h>
#include <musikcore/support/Common.h>
#include <musikcore/support/Settings.h>
#include <musikcore/audio/Buffer.h>
#include <musikcore/audio/Dsp.h>
#include <musikcore/audio/Outputs.h>
#include <musikcore/support/Settings.h>
#include <musikcore/support/SettingKeys.h>
#include <musikcore/plugin/PluginLibraryService.h>
#include <musikcore/library/LibraryBackends.h>
#include <common/runtime/Message.h>
#include <musikcore/support/Messages.h>

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library::query;
using namespace musik::core::runtime;
using namespace musik::core::sdk;

static musik::core::plugin::PlaybackRemotes* playbackRemotes = nullptr;
static std::mutex playbackRemotesMutex;
static std::thread::id playbackRemotesOwner;
static std::thread::id pluginSessionOwner;

static const std::string SUPEREQ_PLUGIN_GUID = "6f0ed53b-0f13-4220-9b0a-ca496b6421cc";

static IMessageQueue* messageQueue = nullptr;
static std::unique_ptr<musik::core::ffi::PluginRegistry> pluginRegistry;
static std::unique_ptr<musik::core::plugin::PluginMediaService> mediaHost;

static void broadcastEqualizerUpdated() {
    if (::messageQueue) {
        ::messageQueue->Broadcast(
            Message::Create(nullptr, message::EqualizerUpdated));
    }
}

static void getEqualizerPluginAndPrefs(
    musik::core::ffi::PluginRegistry::Factory<mc_dsp_factory>& plugin, std::shared_ptr<Settings>& prefs) {
    plugin = musik::core::plugin::FindDspFactory(SUPEREQ_PLUGIN_GUID);
    if (plugin) {
        for (const auto& info : musik::core::plugin::ListPlugins()) {
            if (info.id == SUPEREQ_PLUGIN_GUID) {
                prefs = Settings::ForPlugin(info.name);
                break;
            }
        }
    }
}

static void reloadEqualizer(const musik::core::ffi::PluginRegistry::Factory<mc_dsp_factory>& plugin) {
    plugin.Invoke([](const mc_dsp_factory& factory) {
        factory.reload(factory.context);
        return MC_OK;
    });
}

static constexpr size_t EqualizerBands[] = {
    65, 92, 131, 185, 262, 370, 523, 740, 1047, 1480,
    2093, 2960, 4186, 5920, 8372, 11840, 16744, 22000,
};
static_assert(std::size(EqualizerBands) == MC_EQUALIZER_BAND_COUNT);

static bool writeEqualizerGains(const double* gains) {
    musik::core::ffi::PluginRegistry::Factory<mc_dsp_factory> plugin;
    std::shared_ptr<Settings> prefs;
    getEqualizerPluginAndPrefs(plugin, prefs);
    if (!prefs) {
        return false;
    }
    for (size_t i = 0; i < MC_EQUALIZER_BAND_COUNT; ++i) {
        prefs->SetDouble(std::to_string(EqualizerBands[i]), gains[i]);
    }
    reloadEqualizer(plugin);
    broadcastEqualizerUpdated();
    return true;
}

mc_result MC_CALL mc_equalizer_get_state(mc_equalizer_state* state) {
    if (!state || state->struct_size < sizeof(*state)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return ffi::exception_barrier(nullptr, [&] {
        musik::core::ffi::PluginRegistry::Factory<mc_dsp_factory> plugin;
        std::shared_ptr<Settings> prefs;
        getEqualizerPluginAndPrefs(plugin, prefs);
        if (!prefs) {
            return MC_ERROR_UNSUPPORTED;
        }

        mc_equalizer_state snapshot = MC_EQUALIZER_STATE_DEFAULT;
        snapshot.struct_size = state->struct_size;
        snapshot.enabled = prefs->GetBool("enabled", false);
        for (size_t i = 0; i < MC_EQUALIZER_BAND_COUNT; ++i) {
            snapshot.bands[i] = {
                static_cast<uint32_t>(EqualizerBands[i]), prefs->GetDouble(std::to_string(EqualizerBands[i]), 0.0)
            };
        }
        *state = snapshot;
        return MC_OK;
    });
}

mc_result MC_CALL mc_equalizer_set_enabled(bool enabled) {
    return ffi::exception_barrier(nullptr, [&] {
        musik::core::ffi::PluginRegistry::Factory<mc_dsp_factory> plugin;
        std::shared_ptr<Settings> prefs;
        getEqualizerPluginAndPrefs(plugin, prefs);
        if (!prefs) {
            return MC_ERROR_UNSUPPORTED;
        }
        if (prefs->GetBool("enabled", false) != enabled) {
            prefs->SetBool("enabled", enabled);
            reloadEqualizer(plugin);
            broadcastEqualizerUpdated();
        }
        return MC_OK;
    });
}

mc_result MC_CALL mc_equalizer_set_gains(const double* gains, size_t count) {
    if (!gains || count != MC_EQUALIZER_BAND_COUNT) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!std::isfinite(gains[i]) || gains[i] < -20.0 || gains[i] > 20.0) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
    }
    return ffi::exception_barrier(nullptr, [&] {
        return writeEqualizerGains(gains) ? MC_OK : MC_ERROR_UNSUPPORTED;
    });
}

namespace musik { namespace core { namespace plugin {

    mc_result ValidateCallingThread() {
        std::lock_guard lock(playbackRemotesMutex);
        if (pluginSessionOwner == std::thread::id {}) {
            return MC_ERROR_UNSUPPORTED;
        }
        return pluginSessionOwner == std::this_thread::get_id() ? MC_OK : MC_ERROR_WRONG_THREAD;
    }

    mc_result ApplySettings(std::string_view pluginId) {
        PlaybackRemotes* remotes;
        {
            std::lock_guard lock(playbackRemotesMutex);
            if (!playbackRemotes) {
                return MC_ERROR_UNSUPPORTED;
            }
            if (playbackRemotesOwner != std::this_thread::get_id()) {
                return MC_ERROR_WRONG_THREAD;
            }
            remotes = playbackRemotes;
        }
        return remotes->Reload(pluginId);
    }

    void BindPlaybackRemotes(PlaybackRemotes& remotes) {
        std::lock_guard lock(playbackRemotesMutex);
        musik::contract::Assert(!playbackRemotes, "playback remotes are already bound");
        playbackRemotes = &remotes;
        playbackRemotesOwner = std::this_thread::get_id();
    }

    void UnbindPlaybackRemotes() {
        std::lock_guard lock(playbackRemotesMutex);
        playbackRemotes = nullptr;
        playbackRemotesOwner = {};
    }

    void Initialize(const std::string& pluginDirectory) {
        {
            std::lock_guard lock(playbackRemotesMutex);
            musik::contract::Assert(pluginSessionOwner == std::thread::id {}, "plugin session is already active");
            pluginSessionOwner = std::this_thread::get_id();
        }
        /* keep the first validated directory for later sessions that omit it. */
        static std::string selectedDirectory;
        auto directory = pluginDirectory.empty()
            ? (selectedDirectory.empty() ? GetPluginDirectory() : selectedDirectory)
            : pluginDirectory;
        const auto path = musik::core::sdk::fs::pathFromUtf8(directory);
        if (!pluginDirectory.empty() && !std::filesystem::is_directory(path)) {
            throw std::invalid_argument("Plugin directory does not exist");
        }
        directory = musik::core::sdk::fs::pathToUtf8(std::filesystem::weakly_canonical(path));
        if (!selectedDirectory.empty() && selectedDirectory != directory) {
            throw std::invalid_argument("Plugins have already been discovered in another directory");
        }
        selectedDirectory = directory;

        pluginRegistry = std::make_unique<musik::core::ffi::PluginRegistry>(
            [](mc_log_level level, mc_string_view tag, mc_string_view text) {
                mc_log_write(level, tag, text);
            });
        const auto settings = Settings::ForComponent(prefs::components::Plugins);
        const auto status = pluginRegistry->Initialize(directory, [&](const auto& plugin) {
            const auto leaf = musik::core::sdk::fs::pathToUtf8(std::filesystem::u8path(plugin.filename).filename());
            return settings->GetBool(leaf, true);
        });
        if (status != MC_OK) {
            throw std::runtime_error("Could not initialize C plugin registry: " + std::to_string(status));
        }
        mediaHost = std::make_unique<PluginMediaService>(*pluginRegistry);
    }

    void BindServices(IMessageQueue* queue) {
        ::messageQueue = queue;
    }

    const mc_plugin_media_service& Media() {
        return mediaHost->Api();
    }

    const mc_plugin_audio_scratch_service& AudioScratch() {
        return ffi::audio_scratch_buffers::Host();
    }

    /* finish independent teardown phases even when one phase fails. */
    template<typename Work>
    static void cleanup(Work&& work, bool& failure) noexcept {
        try {
            work();
        }
        catch (...) {
            /* retain the failure, not an exception owned by provider code. */
            failure = true;
        }
    }

    std::vector<ffi::PluginRegistry::PluginInfo> ListPlugins() {
        if (!pluginRegistry) {
            return {};
        }
        return pluginRegistry->ListPlugins();
    }

    std::vector<std::shared_ptr<audio::Dsp>> CreateDsps() {
        return mediaHost ? mediaHost->CreateDsps() : std::vector<std::shared_ptr<audio::Dsp>>();
    }

    ffi::PluginRegistry::Factory<mc_playback_remote_factory> FindPlaybackRemoteFactory(std::string_view pluginId) {
        return pluginRegistry ? pluginRegistry->Find<mc_playback_remote_factory>(pluginId)
            : ffi::PluginRegistry::Factory<mc_playback_remote_factory>();
    }

    ffi::PluginRegistry::Factory<mc_output_factory> FindOutputFactory(std::string_view pluginId) {
        if (!pluginRegistry) {
            return {};
        }
        return pluginRegistry->Find<mc_output_factory>(pluginId);
    }

    ffi::PluginRegistry::Factory<mc_visualizer_factory> FindVisualizerFactory(std::string_view pluginId) {
        return pluginRegistry ? pluginRegistry->Find<mc_visualizer_factory>(pluginId)
            : ffi::PluginRegistry::Factory<mc_visualizer_factory>();
    }

    ffi::PluginRegistry::Factory<mc_analyzer_factory> FindAnalyzerFactory(std::string_view pluginId) {
        return pluginRegistry ? pluginRegistry->Find<mc_analyzer_factory>(pluginId)
            : ffi::PluginRegistry::Factory<mc_analyzer_factory>();
    }

    ffi::PluginRegistry::Factory<mc_tag_reader_factory> FindTagReaderFactory(std::string_view pluginId) {
        return pluginRegistry ? pluginRegistry->Find<mc_tag_reader_factory>(pluginId)
            : ffi::PluginRegistry::Factory<mc_tag_reader_factory>();
    }

    ffi::PluginRegistry::Factory<mc_indexer_source_factory> FindIndexerSourceFactory(std::string_view pluginId) {
        return pluginRegistry ? pluginRegistry->Find<mc_indexer_source_factory>(pluginId)
            : ffi::PluginRegistry::Factory<mc_indexer_source_factory>();
    }

    ffi::PluginRegistry::Factory<mc_dsp_factory> FindDspFactory(std::string_view pluginId) {
        return pluginRegistry ? pluginRegistry->Find<mc_dsp_factory>(pluginId)
            : ffi::PluginRegistry::Factory<mc_dsp_factory>();
    }

    ffi::PluginRegistry::Factory<mc_configuration_api> FindConfiguration(std::string_view pluginId) {
        return pluginRegistry ? pluginRegistry->Find<mc_configuration_api>(pluginId)
            : ffi::PluginRegistry::Factory<mc_configuration_api>();
    }

    static mc_result MC_CALL readInteger(void* context, const mc_integer_setting* setting, int32_t* value) {
        if (!setting || !value || !ffi::valid_text(setting->key, false) ||
            setting->default_value < setting->minimum || setting->default_value > setting->maximum)
        {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return ffi::exception_barrier(nullptr, [&] {
            const std::string key(setting->key.data, setting->key.length);
            const auto saved = static_cast<Settings*>(context)->GetInt(key, setting->default_value);
            *value = saved >= setting->minimum && saved <= setting->maximum ? saved : setting->default_value;
            return MC_OK;
        });
    }

    static mc_result MC_CALL readBoolean(void* context, mc_string_view key, bool fallback, bool* value) {
        if (!value || !ffi::valid_text(key, false)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return ffi::exception_barrier(nullptr, [&] {
            *value = static_cast<Settings*>(context)->GetBool(std::string(key.data, key.length), fallback);
            return MC_OK;
        });
    }

    static mc_result MC_CALL readNumber(void* context, mc_string_view key, double fallback, double* value) {
        if (!value || !std::isfinite(fallback) || !ffi::valid_text(key, false)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return ffi::exception_barrier(nullptr, [&] {
            const auto saved = static_cast<Settings*>(context)->GetDouble(std::string(key.data, key.length), fallback);
            *value = std::isfinite(saved) ? saved : fallback;
            return MC_OK;
        });
    }

    static mc_result MC_CALL readText(
        void* context, mc_string_view key, mc_string_view fallback, mc_setting_text_fn read, void* userData) {
        if (!read || !ffi::valid_text(key, false) || !ffi::valid_text(fallback)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return ffi::exception_barrier(nullptr, [&] {
            const std::string keyText(key.data, key.length);
            const std::string fallbackText(fallback.data ? fallback.data : "", fallback.length);
            const auto value = static_cast<Settings*>(context)->GetString(keyText, fallbackText);
            read(userData, { value.data(), value.size() });
            return MC_OK;
        });
    }

    static mc_result MC_CALL writeValues(void* context, const mc_setting_entry* entries, size_t count) {
        if (!ffi::valid_setting_entries(entries, count)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        if (!count) {
            return MC_OK;
        }
        return ffi::exception_barrier(nullptr, [&] {
            auto values = nlohmann::json::object();
            for (size_t i = 0; i < count; ++i) {
                values[std::string(entries[i].key.data, entries[i].key.length)] =
                    Settings::EncodeValue(entries[i].value);
            }
            return static_cast<Settings*>(context)->SetValues(values, true) ? MC_OK : MC_ERROR_IO;
        });
    }

    mc_plugin_settings_service SettingsService(const std::string& pluginName) {
        return {
            .struct_size = sizeof(mc_plugin_settings_service),
            .context = Settings::ForPlugin(pluginName).get(),
            .read_integer = readInteger,
            .read_boolean = readBoolean,
            .read_number = readNumber,
            .read_text = readText,
            .write_values = writeValues,
        };
    }

    void ValidateShutdown() {
        if (pluginRegistry) {
            pluginRegistry->AssertCanShutdown();
        }
    }

    void UnbindServices() {
        musik::core::plugin::ValidateShutdown();
    }

    void Shutdown() {
        musik::core::plugin::ValidateShutdown();
        bool failure = false;
        cleanup(
            [] {
                Settings::SavePluginSettings();
            },
            failure);
        /* workers have stopped, so cached factories can release provider state. */
        mediaHost.reset();
        musik::contract::Assert(audio::Output::InstanceCount() == 0,
            "output instances must be destroyed before plugin unload");
        cleanup([] {
            if (pluginRegistry) {
                const auto status = pluginRegistry->Shutdown();
                pluginRegistry.reset();
                if (status != MC_OK) {
                    throw std::runtime_error("C provider shutdown failed");
                }
            }
        }, failure);

        ::messageQueue = nullptr;
        {
            std::lock_guard lock(playbackRemotesMutex);
            pluginSessionOwner = {};
        }
        if (failure) {
            throw std::runtime_error("Plugin service shutdown failed");
        }
    }

} } }
