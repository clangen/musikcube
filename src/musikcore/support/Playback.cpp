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
#include "Playback.h"
#include <musikcore/library/query/PersistedPlayQueueQuery.h>
#include <musikcore/sdk/constants.h>
#include <musikcore/support/PreferenceKeys.h>
#include <cmath>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library::query;
using namespace musik::core::sdk;

namespace keys = musik::core::prefs::keys;
using Prefs = std::shared_ptr<Preferences>;

static Prefs Session() {
    return Preferences::ForComponent(prefs::components::Session);
}

static Prefs Settings() {
    return Preferences::ForComponent(prefs::components::Settings);
}

namespace musik {
    namespace core {
        namespace playback {
            void PauseOrResume(ITransport& transport) {
                auto state = transport.GetPlaybackState();
                if (state == PlaybackState::Paused || state == PlaybackState::Prepared) {
                    transport.Resume();
                }
                else if (state == PlaybackState::Playing) {
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

            void LoadPlaybackContext(ILibraryPtr library, PlaybackService& playback) {
                if (Settings()->GetBool(keys::SaveSessionOnExit, true)) {
                    auto prefs = Session();
                    auto query = std::shared_ptr<PersistedPlayQueueQuery>(
                        PersistedPlayQueueQuery::Restore(library, playback));

                    library->Enqueue(query, [&playback, prefs, query](auto q) {
                        int index = prefs->GetInt(keys::LastPlayQueueIndex, -1);
                        if (index >= 0) {
                            double time = prefs->GetDouble(keys::LastPlayQueueTime, 0.0f);
                            playback.Prepare(index, time);
                            playback.QueueEdited(); /* hack to get the play queue view to scroll
                                to the track we just prepared. */

                            if (Settings()->GetBool(keys::ResumePlaybackOnStartup, false)) {
                                playback.GetTransport().Resume();
                            }
                        }
                    });
                }
            }

            void SavePlaybackContext(ILibraryPtr library, PlaybackService& playback) {
                if (Settings()->GetBool(keys::SaveSessionOnExit, true)) {
                    auto prefs = Session();
                    if (playback.GetPlaybackState() != sdk::PlaybackState::Stopped) {
                        prefs->SetInt(keys::LastPlayQueueIndex, (int)playback.GetIndex());

                        /* streams with a negative duration are of indeterminate length,
                        and may be infinite, so don't cache the playback position */
                        double offset = playback.GetDuration() > 0.0 ? playback.GetPosition() : 0.0;
                        prefs->SetDouble(keys::LastPlayQueueTime, offset);
                    }
                    else {
                        prefs->SetInt(keys::LastPlayQueueIndex, -1);
                        prefs->SetDouble(keys::LastPlayQueueTime, 0.0f);
                    }

                    auto query = std::shared_ptr<PersistedPlayQueueQuery>(
                        PersistedPlayQueueQuery::Save(library, playback));

                    library->EnqueueAndWait(query);
                }
            }
        }
    }
}
