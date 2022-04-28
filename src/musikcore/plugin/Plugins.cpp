//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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
#include "Plugins.h"
#include "PluginFactory.h"

#include <musikcore/debug.h>
#include <musikcore/version.h>
#include <musikcore/support/Common.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/io/DataStreamFactory.h>
#include <musikcore/audio/Buffer.h>
#include <musikcore/audio/Streams.h>
#include <musikcore/audio/Outputs.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/library/LocalMetadataProxy.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/support/Messages.h>

#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/IIndexerNotifier.h>
#include <musikcore/sdk/IEnvironment.h>

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library::query;
using namespace musik::core::io;
using namespace musik::core::runtime;
using namespace musik::core::sdk;

typedef void(*SetEnvironment)(IEnvironment*);
typedef void(*SetDebug)(IDebug*);
typedef void(*SetMetadataProxy)(IMetadataProxy*);
typedef void(*SetIndexerNotifier)(IIndexerNotifier*);

static const std::string SUPEREQ_PLUGIN_GUID = "6f0ed53b-0f13-4220-9b0a-ca496b6421cc";

static IMessageQueue* messageQueue = nullptr;
static ILibraryPtr defaultLibrary;
static IPlaybackService* playbackService = nullptr;
static LocalMetadataProxy* metadataProxy = nullptr;
static std::shared_ptr<Preferences> playbackPrefs;

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
    std::shared_ptr<IPlugin>& plugin,
    std::shared_ptr<Preferences>& prefs)
{
    plugin = PluginFactory::Instance().QueryGuid(SUPEREQ_PLUGIN_GUID);

    if (plugin) {
        prefs = Preferences::ForPlugin(plugin->Name());
    }
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
                case PathType::Plugins: path = GetPluginDirectory(); break;
                case PathType::Library: {
                    if (defaultLibrary) {
                        path = GetDataDirectory() + std::to_string(defaultLibrary->Id()) + "/";
                    }
                    break;
                }
            }
            return CopyString(path, dst, size);
        }

        IDataStream* GetDataStream(const char* uri, OpenFlags flags) override {
            return DataStreamFactory::OpenDataStream(uri, flags);
        }

        IDecoder* GetDecoder(IDataStream* stream) override {
            return streams::GetDecoderForDataStream(stream);
        }

        IEncoder* GetEncoder(const char* type) override {
            return streams::GetEncoderForType(type);
        }

        IDebug* GetDebug() override {
            return &debugger;
        }

        IPreferences* GetPreferences(const char* name) override {
            return Preferences::Unmanaged(name ? name : std::string());
        }

        IBuffer* GetBuffer(size_t samples, size_t rate = 44100, size_t channels = 2) override {
            musik::core::audio::Buffer* buffer = new Buffer();
            buffer->SetChannels(2);
            buffer->SetSampleRate((long) rate);
            buffer->SetSamples((long) samples);
            return buffer;
        }

        size_t GetOutputCount() override {
            return outputs::GetOutputCount();
        }

        IOutput* GetOutputAtIndex(size_t index) override {
            return outputs::GetUnmanagedOutput(index);
        }

        IOutput* GetOutputWithName(const char* name) override {
            return outputs::GetUnmanagedOutput(name ? name : "");
        }

        void SetDefaultOutput(IOutput* output) override {
            if (output) {
                auto current = outputs::SelectedOutput();
                std::string newName = output->Name();
                std::string currentName = current ? current->Name() : "";
                auto newDevice = output->GetDefaultDevice();
                auto currentDevice = current->GetDefaultDevice();
                std::string newDeviceId = newDevice ? newDevice->Id() : "";
                std::string currentDeviceId = currentDevice ? currentDevice->Id() : "";
                if (newName != currentName || newDeviceId != currentDeviceId) {
                    outputs::SelectOutput(output);
                    if (::playbackService) {
                        playbackService->ReloadOutput();
                    }
                }
                saveEnvironment();
            }
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

        IOutput* GetDefaultOutput() override {
            return outputs::GetUnmanagedSelectedOutput();
        }

        void ReindexMetadata() override {
            if (::defaultLibrary) {
                ::defaultLibrary->Indexer()->Schedule(IIndexer::SyncType::Local);
            }
        }

        void RebuildMetadata() override {
            if (::defaultLibrary) {
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
            if (count != EqualizerBandCount) {
                return false;
            }

            std::shared_ptr<IPlugin> plugin;
            std::shared_ptr<Preferences> prefs;
            getEqualizerPluginAndPrefs(plugin, prefs);

            if (plugin && prefs) {
                for (size_t i = 0; i < EqualizerBandCount; i++) {
                    target[i] = prefs->GetDouble(std::to_string(EqualizerBands[i]), 0.0);
                }

                return true;
            }

            return false;
        }

        bool SetEqualizerBandValues(double values[], size_t count) override {
            if (count != EqualizerBandCount) {
                return false;
            }

            std::shared_ptr<IPlugin> plugin;
            std::shared_ptr<Preferences> prefs;
            getEqualizerPluginAndPrefs(plugin, prefs);

            if (plugin && prefs) {
                for (size_t i = 0; i < EqualizerBandCount; i++) {
                    prefs->SetDouble(std::to_string(EqualizerBands[i]), values[i]);
                }
                plugin->Reload();
                broadcastEqualizerUpdated();
                return true;
            }

            return false;
        }

        bool GetEqualizerEnabled() override {
            std::shared_ptr<IPlugin> plugin;
            std::shared_ptr<Preferences> prefs;
            getEqualizerPluginAndPrefs(plugin, prefs);

            if (plugin && prefs) {
                return prefs->GetBool("enabled", false);
            }

            return false;
        }

        void SetEqualizerEnabled(bool enabled) override {
            std::shared_ptr<IPlugin> plugin;
            std::shared_ptr<Preferences> prefs;
            getEqualizerPluginAndPrefs(plugin, prefs);

            if (plugin && prefs) {
                if (prefs->GetBool("enabled", false) != enabled) {
                    prefs->SetBool("enabled", enabled);
                    plugin->Reload();
                    broadcastEqualizerUpdated();
                }
            }
        }

        void ReloadPlaybackOutput() override {
            if (playbackService) {
                playbackService->ReloadOutput();
            }
        }

        const char* GetAppVersion() override {
            return VERSION;
        }

} environment;

namespace musik { namespace core { namespace plugin {

    void Init() {
        /* preferences */
        Preferences::LoadPluginPreferences();

        /* debug */
        PluginFactory::Instance().QueryFunction<SetDebug>(
            "SetDebug",
            [](musik::core::sdk::IPlugin* plugin, SetDebug func) {
            func(&debugger);
        });
    }

    void Start(IMessageQueue* messageQueue, IPlaybackService* playbackService, ILibraryPtr library) {
        /* metadata proxies */
        delete metadataProxy;
        ::messageQueue = messageQueue;
        ::defaultLibrary = library;
        ::playbackService = playbackService;
        ::playbackPrefs = Preferences::ForComponent(prefs::components::Playback);

        /* even if the local client is connected to a remote server, the metadata proxy
        always uses the default local library. */
        ::metadataProxy = new LocalMetadataProxy(LibraryFactory::Instance().DefaultLocalLibrary());

        PluginFactory::Instance().QueryFunction<SetMetadataProxy>(
            "SetMetadataProxy",
            [](musik::core::sdk::IPlugin* plugin, SetMetadataProxy func) {
                func(metadataProxy);
            });

        /* indexer */
        IIndexerNotifier* indexerNotifier =
            dynamic_cast<IIndexerNotifier*>(library->Indexer());

        PluginFactory::Instance().QueryFunction<SetIndexerNotifier>(
            "SetIndexerNotifier",
            [indexerNotifier](musik::core::sdk::IPlugin* plugin, SetIndexerNotifier func) {
                func(indexerNotifier);
            });

        /* environment */
        PluginFactory::Instance().QueryFunction<SetEnvironment>(
            "SetEnvironment",
            [](musik::core::sdk::IPlugin* plugin, SetEnvironment func) {
                func(&environment);
            });
    }

    IEnvironment& Environment() {
        return environment;
    }

    void Shutdown() {
        /* preferences */
        Preferences::SavePluginPreferences();

        /* data providers */
        PluginFactory::Instance().QueryFunction<SetMetadataProxy>(
            "SetMetadataProxy",
            [](musik::core::sdk::IPlugin* plugin, SetMetadataProxy func) {
                func(nullptr);
            });

        delete metadataProxy;
        ::messageQueue = nullptr;
        ::metadataProxy = nullptr;
        ::defaultLibrary.reset();
        ::playbackService = nullptr;
        ::playbackPrefs.reset();

        /* indexer */
        PluginFactory::Instance().QueryFunction<SetIndexerNotifier>(
            "SetIndexerNotifier",
                [](musik::core::sdk::IPlugin* plugin, SetIndexerNotifier func) {
                func(nullptr);
            });

        /* environment */
        PluginFactory::Instance().QueryFunction<SetEnvironment>(
            "SetEnvironment",
            [](musik::core::sdk::IPlugin* plugin, SetEnvironment func) {
                func(nullptr);
            });

        /* debug */
        PluginFactory::Instance().QueryFunction<SetDebug>(
            "SetDebug",
            [](musik::core::sdk::IPlugin* plugin, SetDebug func) {
                func(&nullDebugger);
            });
    }

} } }
