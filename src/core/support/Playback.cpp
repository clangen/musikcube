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
#include "Playback.h"
#include <core/library/query/local/PersistedPlayQueueQuery.h>
#include <core/sdk/constants.h>
#include <core/support/PreferenceKeys.h>
#include <cmath>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db::local;
using namespace musik::core::sdk;

namespace keys = musik::core::prefs::keys;
using Prefs = std::shared_ptr<Preferences>;

namespace musik {
    namespace core {
        namespace playback {
            void PauseOrResume(ITransport& transport) {
                int state = transport.GetPlaybackState();
                if (state == PlaybackPaused || state == PlaybackPrepared) {
                    transport.Resume();
                }
                else if (state == PlaybackPlaying) {
                    transport.Pause();
                }
            }

            void VolumeUp(ITransport& transport) {
                double delta = round(transport.Volume() * 100.0) >= 10.0 ? 0.05 : 0.01;
                transport.SetVolume(transport.Volume() + delta);
            }

            void VolumeDown(ITransport& transport) {
                double delta = round(transport.Volume() * 100.0) > 10.0 ? 0.05 : 0.01;
                transport.SetVolume(transport.Volume() - delta);
            }

            void SeekForward(IPlaybackService& playback) {
                playback.SetPosition(playback.GetPosition() + 10.0f);
            }

            void SeekBack(IPlaybackService& playback) {
                playback.SetPosition(playback.GetPosition() - 10.0f);
            }

            void SeekForwardProportional(IPlaybackService& playback) {
                double moveBy = 0.05f * playback.GetDuration();
                playback.SetPosition(playback.GetPosition() + moveBy);
            }

            void SeekBackProportional(IPlaybackService& playback) {
                double moveBy = 0.05f * playback.GetDuration();
                playback.SetPosition(playback.GetPosition() - moveBy);
            }

            void LoadPlaybackContext(Prefs prefs, ILibraryPtr library, PlaybackService& playback) {
                if (prefs->GetBool(keys::SaveSessionOnExit, true)) {
                    auto query = std::shared_ptr<PersistedPlayQueueQuery>(
                        PersistedPlayQueueQuery::Restore(library, playback));

                    library->Enqueue(query, ILibrary::QuerySynchronous);

                    int index = prefs->GetInt(keys::LastPlayQueueIndex, -1);
                    if (index >= 0) {
                        double time = prefs->GetDouble(keys::LastPlayQueueTime, 0.0f);
                        playback.Prepare(index, time);
                    }
                }
            }

            void SavePlaybackContext(Prefs prefs, ILibraryPtr library, PlaybackService& playback) {
                if (prefs->GetBool(keys::SaveSessionOnExit, true)) {
                    if (playback.GetPlaybackState() != sdk::PlaybackStopped) {
                        prefs->SetInt(keys::LastPlayQueueIndex, (int)playback.GetIndex());
                        prefs->SetDouble(keys::LastPlayQueueTime, playback.GetPosition());
                    }
                    else {
                        prefs->SetInt(keys::LastPlayQueueIndex, -1);
                        prefs->SetDouble(keys::LastPlayQueueTime, 0.0f);
                    }

                    auto query = std::shared_ptr<PersistedPlayQueueQuery>(
                        PersistedPlayQueueQuery::Save(library, playback));

                    library->Enqueue(query, ILibrary::QuerySynchronous);
                }
            }
        }
    }
}
