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
#include "PluginFactory.h"
#include "MediaHost.h"
#include "PlaybackRemotes.h"
#include <musikcore/ffi/plugin_abi/AudioScratchBuffers.h>
#include <musikcore/equalizer.h>
#include "../ffi/common/error.h"
#include "../ffi/common/primitives.h"
#include <cmath>
#include <filesystem>
#include <mutex>
#include <thread>
#include <musikcore/sdk/Filesystem.h>
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>

#include <musikcore/debug.h>
#include <common/Contract.h>
#include <musikcore/sdk/version.h>
#include <musikcore/support/Common.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/audio/Buffer.h>
#include <musikcore/audio/Dsp.h>
#include <musikcore/audio/Outputs.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/library/LibraryHost.h>
#include <musikcore/library/LibraryFactory.h>
#include <common/runtime/Message.h>
#include <musikcore/support/Messages.h>

#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/IEnvironment.h>

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
typedef void(*SetEnvironment)(IEnvironment*);
typedef void(*SetDebug)(IDebug*);

static const std::string SUPEREQ_PLUGIN_GUID = "6f0ed53b-0f13-4220-9b0a-ca496b6421cc";

static IMessageQueue* messageQueue = nullptr;
static ILibraryPtr defaultLibrary;
static IPlaybackService* playbackService = nullptr;
static std::shared_ptr<Preferences> playbackPrefs;
static std::unique_ptr<musik::core::ffi::PluginRegistry> pluginRegistry;
static std::unique_ptr<musik::core::plugin::MediaHost> mediaHost;

static void saveEnvironment() {
    if (::playbackPrefs) {
        ::playbackPrefs->Save();
    }
    if (::messageQueue) {
        ::messageQueue->Broadcast(
            Message::Create(nullptr, message::EnvironmentUpdated));
    }
}

static void broadcastEqualizerUpdated() {
    if (::messageQueue) {
        ::messageQueue->Broadcast(
            Message::Create(nullptr, message::EqualizerUpdated));
    }
}

static void getEqualizerPluginAndPrefs(
    musik::core::ffi::PluginRegistry::Factory<mc_dsp_factory>& plugin,
    std::shared_ptr<Preferences>& prefs)
{
    plugin = musik::core::plugin::FindDspFactory(SUPEREQ_PLUGIN_GUID);
    if (plugin) {
        for (const auto& info : musik::core::plugin::ListPlugins()) {
            if (info.id == SUPEREQ_PLUGIN_GUID) {
                prefs = Preferences::ForPlugin(info.name);
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

static_assert(EqualizerBandCount == MC_EQUALIZER_BAND_COUNT);

static bool writeEqualizerGains(const double* gains) {
    musik::core::ffi::PluginRegistry::Factory<mc_dsp_factory> plugin;
    std::shared_ptr<Preferences> prefs;
    getEqualizerPluginAndPrefs(plugin, prefs);
    if (!prefs) {
        return false;
    }
    for (size_t i = 0; i < EqualizerBandCount; ++i) {
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
        std::shared_ptr<Preferences> prefs;
        getEqualizerPluginAndPrefs(plugin, prefs);
        if (!prefs) {
            return MC_ERROR_UNSUPPORTED;
        }

        mc_equalizer_state snapshot = MC_EQUALIZER_STATE_DEFAULT;
        snapshot.struct_size = state->struct_size;
        snapshot.enabled = prefs->GetBool("enabled", false);
        for (size_t i = 0; i < EqualizerBandCount; ++i) {
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
        std::shared_ptr<Preferences> prefs;
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

static class Debug: public IDebug {
    public:
        void Verbose(const char* tag, const char* message) override {
            musik::debug::verbose(tag, message);
        }

        void Info(const char* tag, const char* message) override {
            musik::debug::info(tag, message);
        }

        void Warning(const char* tag, const char* message) override {
            musik::debug::warning(tag, message);
        }

        void Error(const char* tag, const char* message) override {
            musik::debug::error(tag, message);
        }
} debugger;

static class NullDebug: public IDebug { /* used during shutdown */
    public:
        void Verbose(const char* tag, const char* message) override {}
        void Info(const char* tag, const char* message) override {}
        void Warning(const char* tag, const char* message) override {}
        void Error(const char* tag, const char* message) override {}
} nullDebugger;

static class Environment: public IEnvironment {
    public:
        size_t GetPath(PathType type, char* dst, int size) override {
            std::string path;
            switch (type) {
                case PathType::UserHome: path = GetHomeDirectory(); break;
                case PathType::Data: path = GetDataDirectory(); break;
                case PathType::Application: path = GetApplicationDirectory(); break;
                case PathType::Plugins: path = PluginFactory::Instance().Directory(); break;
                case PathType::Library: {
                    if (defaultLibrary) {
                        path = GetDataDirectory() + std::to_string(defaultLibrary->Id()) + "/";
                    }
                    break;
                }
            }
            return CopyString(path, dst, size);
        }

        const mc_media_host_api& Media() override { return mediaHost->Api(); }

        const mc_audio_scratch_buffer_api& AudioScratch() override { return ffi::audio_scratch_buffers::Host(); }


        IDebug* GetDebug() override {
            return &debugger;
        }

        IPreferences* GetPreferences(const char* name) override {
            return Preferences::Unmanaged(name ? name : std::string());
        }

        mc_result GetOutputConfiguration(mc_string_view driver, mc_output_configuration_fn read, void* data) override {
            return outputs::ReadConfiguration(driver, read, data);
        }

        mc_result SelectOutput(mc_string_view driver, mc_string_view device) override {
            const auto result = outputs::Configure(driver, device);
            if (result == MC_OK && ::playbackService) {
                ::playbackService->ReloadOutput();
                saveEnvironment();
            }
            return result;
        }

        TransportType GetTransportType() override {
            if (::playbackPrefs) {
                return (TransportType) ::playbackPrefs->GetInt(
                    prefs::keys::Transport.c_str(), (int) TransportType::Gapless);
            }
            return TransportType::Gapless;
        }

        void SetTransportType(TransportType type) override {
            if (::playbackPrefs) {
                auto currentType = GetTransportType();
                if (currentType != type) {
                    ::playbackPrefs->SetInt(prefs::keys::Transport.c_str(), (int) type);
                    if (::playbackService) {
                        ::playbackService->ReloadOutput();
                    }
                    saveEnvironment();
                }
            }
        }

        void ReindexMetadata() override {
            if (::defaultLibrary && ::defaultLibrary->Indexer()) {
                ::defaultLibrary->Indexer()->Schedule(IIndexer::SyncType::Local);
            }
        }

        void RebuildMetadata() override {
            if (::defaultLibrary && ::defaultLibrary->Indexer()) {
                ::defaultLibrary->Indexer()->Schedule(IIndexer::SyncType::Rebuild);
            }
        }

        ReplayGainMode GetReplayGainMode() override {
            if (::playbackPrefs) {
                return (ReplayGainMode) ::playbackPrefs->GetInt(
                    prefs::keys::ReplayGainMode.c_str(),
                    (int) ReplayGainMode::Disabled);
            }
            return ReplayGainMode::Disabled;
        }

        void SetReplayGainMode(ReplayGainMode mode) override {
            if (::playbackPrefs) {
                ::playbackPrefs->SetInt(prefs::keys::ReplayGainMode.c_str(), (int) mode);
                saveEnvironment();
            }
        }

        float GetPreampGain() override {
            if (::playbackPrefs) {
                return (float) ::playbackPrefs->GetDouble(
                    prefs::keys::PreampDecibels.c_str(), 0.0f);
            }
            return 1.0f;
        }

        void SetPreampGain(float gain) override {
            if (::playbackPrefs) {
                if (gain > 20.0f) { gain = 20.0f; }
                if (gain < -20.0f) { gain = -20.0f; }
                ::playbackPrefs->SetDouble(prefs::keys::PreampDecibels.c_str(), gain);
                saveEnvironment();
            }
        }

        bool GetEqualizerBandValues(double target[], size_t count) override {
            mc_equalizer_state state = MC_EQUALIZER_STATE_DEFAULT;
            if (!target || count != EqualizerBandCount || mc_equalizer_get_state(&state) != MC_OK) {
                return false;
            }
            for (size_t i = 0; i < count; ++i) {
                target[i] = state.bands[i].gain;
            }
            return true;
        }

        bool SetEqualizerBandValues(double values[], size_t count) override {
            return values && count == EqualizerBandCount && writeEqualizerGains(values);
        }

        bool GetEqualizerEnabled() override {
            mc_equalizer_state state = MC_EQUALIZER_STATE_DEFAULT;
            return mc_equalizer_get_state(&state) == MC_OK && state.enabled;
        }

        void SetEqualizerEnabled(bool enabled) override {
            mc_equalizer_set_enabled(enabled);
        }

        void ReloadPlaybackOutput() override {
            if (playbackService) {
                playbackService->ReloadOutput();
            }
        }

        const char* GetAppVersion() override {
            return MUSIKCUBE_VERSION;
        }

} environment;

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
        auto& factory = PluginFactory::Instance(pluginDirectory);
        factory.BeginSession();

        /* preferences */
        Preferences::LoadPluginPreferences();

        /* debug */
        PluginFactory::Instance().QueryFunction<SetDebug>(
            "SetDebug",
            [](musik::core::sdk::IPlugin* plugin, SetDebug func) {
            func(&debugger);
        }, PluginFactory::FunctionSelection::Session);

        /* discover C plugins for this engine session. the registry owns selected
        DLLs until their users finish and session teardown closes them. */
        pluginRegistry = std::make_unique<musik::core::ffi::PluginRegistry>(
            [](mc_log_level level, mc_string_view tag, mc_string_view text) {
                mc_log_write(level, tag, text);
            });
        const auto preferences = Preferences::ForComponent(prefs::components::Plugins);
        /* both contracts use the process-selected directory, including when
        an embedded host omits its directory on a later session. */
        const auto status = pluginRegistry->Initialize(factory.Directory(), [&](const auto& plugin) {
            const auto leaf = musik::core::sdk::fs::pathToUtf8(std::filesystem::u8path(plugin.filename).filename());
            return preferences->GetBool(leaf, true);
        });
        if (status != MC_OK) {
            throw std::runtime_error("Could not initialize C plugin registry: " + std::to_string(status));
        }
        mediaHost = std::make_unique<MediaHost>(*pluginRegistry);
    }

    void BindServices(IMessageQueue* messageQueue, IPlaybackService* playbackService, ILibraryPtr library) {
        ::messageQueue = messageQueue;
        ::defaultLibrary = library;
        ::playbackService = playbackService;
        ::playbackPrefs = Preferences::ForComponent(prefs::components::Playback);


        /* environment */
        PluginFactory::Instance().QueryFunction<SetEnvironment>(
            "SetEnvironment",
            [](musik::core::sdk::IPlugin* plugin, SetEnvironment func) {
                func(&environment);
            }, PluginFactory::FunctionSelection::Session);
    }

    IEnvironment& Environment() {
        return environment;
    }

    /* finish independent teardown phases even when a legacy hook throws. */
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
            const auto saved = static_cast<Preferences*>(context)->GetInt(key, setting->default_value);
            *value = saved >= setting->minimum && saved <= setting->maximum ? saved : setting->default_value;
            return MC_OK;
        });
    }

    static mc_result MC_CALL readBoolean(void* context, mc_string_view key, bool fallback, bool* value) {
        if (!value || !ffi::valid_text(key, false)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return ffi::exception_barrier(nullptr, [&] {
            *value = static_cast<Preferences*>(context)->GetBool(std::string(key.data, key.length), fallback);
            return MC_OK;
        });
    }

    static mc_result MC_CALL readNumber(void* context, mc_string_view key, double fallback, double* value) {
        if (!value || !std::isfinite(fallback) || !ffi::valid_text(key, false)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return ffi::exception_barrier(nullptr, [&] {
            const auto saved = static_cast<Preferences*>(context)->GetDouble(std::string(key.data, key.length), fallback);
            *value = std::isfinite(saved) ? saved : fallback;
            return MC_OK;
        });
    }

    static mc_result MC_CALL readText(void* context, mc_string_view key, mc_string_view fallback,
        mc_preference_text_fn read, void* userData)
    {
        if (!read || !ffi::valid_text(key, false) || !ffi::valid_text(fallback)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return ffi::exception_barrier(nullptr, [&] {
            const std::string keyText(key.data, key.length);
            const std::string fallbackText(fallback.data ? fallback.data : "", fallback.length);
            const auto value = static_cast<Preferences*>(context)->GetString(keyText, fallbackText);
            read(userData, { value.data(), value.size() });
            return MC_OK;
        });
    }

    mc_preferences_host_api ReadSettingsService(const std::string& pluginName) {
        return {
            .struct_size = sizeof(mc_preferences_host_api),
            .context = Preferences::ForPlugin(pluginName).get(),
            .read_integer = readInteger,
            .read_boolean = readBoolean,
            .read_number = readNumber,
            .read_text = readText,
        };
    }

    void ValidateShutdown() {
        if (pluginRegistry) {
            pluginRegistry->AssertCanShutdown();
        }
    }

    void UnbindServices() {
        musik::core::plugin::ValidateShutdown();
        try {
            PluginFactory::Instance().QueryFunction<SetEnvironment>(
                "SetEnvironment", [](musik::core::sdk::IPlugin*, SetEnvironment function) {
                    function(nullptr);
                }, PluginFactory::FunctionSelection::Session);
            PluginFactory::Instance().QueryFunction<SetDebug>(
                "SetDebug", [](musik::core::sdk::IPlugin*, SetDebug function) {
                    function(&nullDebugger);
                }, PluginFactory::FunctionSelection::Session);
        }
        catch (...) {
            /* a failed host detachment cannot prove that plugin workers stopped
            using playback/library state. do not destroy services or unload code. */
            musik::contract::Assert(false, "plugin host detachment failed; worker quiescence is not safe");
        }
        ::playbackService = nullptr;
    }

    void Shutdown() {
        musik::core::plugin::ValidateShutdown();
        bool failure = false;
        cleanup([] { Preferences::SavePluginPreferences(); }, failure);
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
        cleanup([] { PluginFactory::Instance().EndSession(); }, failure);

        ::messageQueue = nullptr;
        ::defaultLibrary.reset();
        ::playbackService = nullptr;
        ::playbackPrefs.reset();
        {
            std::lock_guard lock(playbackRemotesMutex);
            pluginSessionOwner = {};
        }
        if (failure) {
            throw std::runtime_error("Plugin service shutdown failed");
        }
    }

} } }
