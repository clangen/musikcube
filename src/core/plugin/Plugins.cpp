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
#include "Plugins.h"
#include "PluginFactory.h"

#include <core/support/Common.h>
#include <core/support/Preferences.h>

#include <core/debug.h>
#include <core/io/DataStreamFactory.h>
#include <core/audio/Buffer.h>
#include <core/audio/Streams.h>
#include <core/audio/Outputs.h>
#include <core/support/Preferences.h>
#include <core/support/PreferenceKeys.h>
#include <core/library/LocalMetadataProxy.h>
#include <core/runtime/Message.h>
#include <core/support/Messages.h>

#include <core/sdk/IDebug.h>
#include <core/sdk/IIndexerNotifier.h>
#include <core/sdk/IEnvironment.h>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db::local;
using namespace musik::core::io;
using namespace musik::core::runtime;
using namespace musik::core::sdk;

typedef void(*SetEnvironment)(IEnvironment*);
typedef void(*SetDebug)(IDebug*);
typedef void(*SetMetadataProxy)(IMetadataProxy*);
typedef void(*SetIndexerNotifier)(IIndexerNotifier*);

static const std::string SUPEREQ_PLUGIN_GUID = "6f0ed53b-0f13-4220-9b0a-ca496b6421cc";

static IMessageQueue* messageQueue = nullptr;
static ILibraryPtr library;
static IPlaybackService* playback = nullptr;
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
        virtual void Verbose(const char* tag, const char* message) override {
            musik::debug::verbose(tag, message);
        }

        virtual void Info(const char* tag, const char* message) override {
            musik::debug::info(tag, message);
        }

        virtual void Warning(const char* tag, const char* message) override {
            musik::debug::warning(tag, message);
        }

        virtual void Error(const char* tag, const char* message) override {
            musik::debug::error(tag, message);
        }
} debugger;

static class Environment: public IEnvironment {
    public:
        virtual size_t GetPath(PathType type, char* dst, int size) override {
            std::string path;
            switch (type) {
                case PathUserHome: path = GetHomeDirectory(); break;
                case PathData: path = GetDataDirectory(); break;
                case PathApplication: path = GetApplicationDirectory(); break;
                case PathPlugins: path = GetPluginDirectory(); break;
                case PathLibrary: {
                    if (library) {
                        path = GetDataDirectory() + std::to_string(library->Id()) + "/";
                    }
                    break;
                }
            }
            return CopyString(path, dst, size);
        }

        virtual IDataStream* GetDataStream(const char* uri, OpenFlags flags) override {
            return DataStreamFactory::OpenDataStream(uri, flags);
        }

        virtual IDecoder* GetDecoder(IDataStream* stream) override {
            return streams::GetDecoderForDataStream(stream);
        }

        virtual IEncoder* GetEncoder(const char* type) override {
            return streams::GetEncoderForType(type);
        }

        virtual IDebug* GetDebug() override {
            return &debugger;
        }

        virtual IPreferences* GetPreferences(const char* name) override {
            return Preferences::Unmanaged(name ? name : std::string());
        }

        virtual IBuffer* GetBuffer(size_t samples, size_t rate = 44100, size_t channels = 2) override {
            musik::core::audio::Buffer* buffer = new Buffer();
            buffer->SetChannels(2);
            buffer->SetSampleRate(rate);
            buffer->SetSamples(samples);
            return buffer;
        }

        virtual size_t GetOutputCount() override {
            return outputs::GetOutputCount();
        }

        virtual IOutput* GetOutputAtIndex(size_t index) override {
            return outputs::GetUnmanagedOutput(index);
        }

        virtual IOutput* GetOutputWithName(const char* name) override {
            return outputs::GetUnmanagedOutput(name ? name : "");
        }

        virtual void SetDefaultOutput(IOutput* output) override {
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
                    if (::playback) {
                        playback->ReloadOutput();
                    }
                }
                saveEnvironment();
            }
        }

        virtual TransportType GetTransportType() override {
            if (::playbackPrefs) {
                return (TransportType) ::playbackPrefs->GetInt(
                    prefs::keys::Transport.c_str(), (int) TransportType::Gapless);
            }
            return TransportType::Gapless;
        }

        virtual void SetTransportType(TransportType type) override {
            if (::playbackPrefs) {
                auto currentType = GetTransportType();
                if (currentType != type) {
                    ::playbackPrefs->SetInt(prefs::keys::Transport.c_str(), (int) type);
                    if (::playback) {
                        ::playback->ReloadOutput();
                    }
                    saveEnvironment();
                }
            }
        }

        virtual IOutput* GetDefaultOutput() override {
            return outputs::GetUnmanagedSelectedOutput();
        }

        virtual void ReindexMetadata() override {
            if (::library) {
                ::library->Indexer()->Schedule(IIndexer::SyncType::Local);
            }
        }

        virtual void RebuildMetadata() override {
            if (::library) {
                ::library->Indexer()->Schedule(IIndexer::SyncType::Rebuild);
            }
        }

        virtual ReplayGainMode GetReplayGainMode() override {
            if (::playbackPrefs) {
                return (ReplayGainMode) ::playbackPrefs->GetInt(
                    prefs::keys::ReplayGainMode.c_str(),
                    (int) ReplayGainMode::Disabled);
            }
            return ReplayGainMode::Disabled;
        }

        virtual void SetReplayGainMode(ReplayGainMode mode) override {
            if (::playbackPrefs) {
                ::playbackPrefs->SetInt(prefs::keys::ReplayGainMode.c_str(), (int) mode);
                saveEnvironment();
            }
        }

        virtual float GetPreampGain() override {
            if (::playbackPrefs) {
                return (float) ::playbackPrefs->GetDouble(
                    prefs::keys::PreampDecibels.c_str(), 0.0f);
            }
            return 1.0f;
        }

        virtual void SetPreampGain(float gain) override {
            if (::playbackPrefs) {
                if (gain > 20.0f) { gain = 20.0f; }
                if (gain < -20.0f) { gain = -20.0f; }
                ::playbackPrefs->SetDouble(prefs::keys::PreampDecibels.c_str(), gain);
                saveEnvironment();
            }
        }

        virtual bool GetEqualizerBandValues(double target[], size_t count) override {
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

        virtual bool SetEqualizerBandValues(double values[], size_t count) override {
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

        virtual bool GetEqualizerEnabled() override {
            std::shared_ptr<IPlugin> plugin;
            std::shared_ptr<Preferences> prefs;
            getEqualizerPluginAndPrefs(plugin, prefs);

            if (plugin && prefs) {
                return prefs->GetBool("enabled", false);
            }

            return false;
        }


        virtual void SetEqualizerEnabled(bool enabled) override {
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

        virtual void ReloadPlaybackOutput() override {
            if (playback) {
                playback->ReloadOutput();
            }
        }
} environment;

namespace musik { namespace core { namespace plugin {

    void Init(IMessageQueue* messageQueue, IPlaybackService* playback, ILibraryPtr library) {
        /* preferences */
        Preferences::LoadPluginPreferences();

        /* debug */
        PluginFactory::Instance().QueryFunction<SetDebug>(
            "SetDebug",
            [](musik::core::sdk::IPlugin* plugin, SetDebug func) {
            func(&debugger);
        });

        /* metadata proxies */
        delete metadataProxy;
        ::messageQueue = messageQueue;
        ::library = library;
        ::playback = playback;
        ::metadataProxy = new LocalMetadataProxy(library);
        ::playbackPrefs = Preferences::ForComponent(prefs::components::Playback);

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

    void Deinit() {
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
        ::library.reset();
        ::playback = nullptr;
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
                func(nullptr);
            });
    }

} } }
