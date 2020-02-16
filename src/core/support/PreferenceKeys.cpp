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
#include "PreferenceKeys.h"

namespace musik { namespace core { namespace prefs {

    const std::string components::Settings = "settings";
    const std::string components::Libraries = "libraries";
    const std::string components::Playback = "playback";
    const std::string components::Plugins = "plugins";
    const std::string components::Session = "session";

    const std::string keys::AutoSyncIntervalMillis = "AutoSyncIntervalMillis";
    const std::string keys::MaxTagReadThreads = "MaxTagReadThreads";
    const std::string keys::RemoveMissingFiles = "RemoveMissingFiles";
    const std::string keys::SyncOnStartup = "SyncOnStartup";
    const std::string keys::Volume = "Volume";
    const std::string keys::RepeatMode = "RepeatMode";
    const std::string keys::TimeChangeMode = "TimeChangeMode";
    const std::string keys::OutputPlugin = "OutputPlugin";
    const std::string keys::Transport = "Transport";
    const std::string keys::Locale = "Locale";
    const std::string keys::IndexerLogEnabled = "IndexerLogEnabled";
    const std::string keys::ReplayGainMode = "ReplayGainMode";
    const std::string keys::PreampDecibels = "PreampDecibels";
    const std::string keys::SaveSessionOnExit = "SaveSessionOnExit";
    const std::string keys::LastPlayQueueIndex = "LastPlayQueueIndex";
    const std::string keys::LastPlayQueueTime = "LastPlayQueueTime";
    const std::string keys::LastFmToken = "LastFmToken";
    const std::string keys::LastFmSessionId = "LastFmSessionId";
    const std::string keys::LastFmUsername = "LastFmUsername";
    const std::string keys::DisableAlbumArtistFallback = "DisableAlbumArtistFallback";
    const std::string keys::AuddioApiToken = "AuddioApiToken";

} } }

