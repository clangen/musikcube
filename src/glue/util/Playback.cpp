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
#include <core/sdk/constants.h>
#include <cmath>

using namespace musik::core::audio;
using namespace musik::core::sdk;

namespace musik {
    namespace glue {
        namespace playback {
            void PauseOrResume(ITransport& transport) {
                int state = transport.GetPlaybackState();
                if (state == PlaybackPaused) {
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
        }
    }
}
