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

#pragma once

#include <musikcore/library/LibraryBackend.h>
#include <common/runtime/IMessage.h>
#include <string>
#include <string_view>
#include <vector>
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>

namespace musik { namespace core { namespace audio { class Dsp; } } }

namespace musik { namespace core { namespace plugin {

    class PlaybackRemotes;

    mc_result ValidateCallingThread();

    void Initialize(const std::string& pluginDirectory);

    void BindServices(musik::core::runtime::IMessageQueue* messageQueue);

    mc_result ApplySettings(std::string_view pluginId);

    void BindPlaybackRemotes(PlaybackRemotes& remotes);
    void UnbindPlaybackRemotes();

    const mc_plugin_media_service& Media();
    const mc_plugin_audio_scratch_service& AudioScratch();

    std::vector<std::shared_ptr<audio::Dsp>> CreateDsps();

    std::vector<ffi::PluginRegistry::PluginInfo> ListPlugins();

    ffi::PluginRegistry::Factory<mc_playback_remote_factory> FindPlaybackRemoteFactory(std::string_view pluginId);

    ffi::PluginRegistry::Factory<mc_output_factory> FindOutputFactory(std::string_view pluginId);
    ffi::PluginRegistry::Factory<mc_visualizer_factory> FindVisualizerFactory(std::string_view pluginId);

    ffi::PluginRegistry::Factory<mc_analyzer_factory> FindAnalyzerFactory(std::string_view pluginId);

    ffi::PluginRegistry::Factory<mc_tag_reader_factory> FindTagReaderFactory(std::string_view pluginId);
    ffi::PluginRegistry::Factory<mc_indexer_source_factory> FindIndexerSourceFactory(std::string_view pluginId);

    ffi::PluginRegistry::Factory<mc_dsp_factory> FindDspFactory(std::string_view pluginId);

    ffi::PluginRegistry::Factory<mc_configuration_api> FindConfiguration(std::string_view pluginId);

    mc_plugin_preferences_service ReadSettingsService(const std::string& pluginName);

    void ValidateShutdown();
    void UnbindServices();
    void Shutdown();

} } }
