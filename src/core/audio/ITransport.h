//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <core/config.h>
#include <sigslot/sigslot.h>

namespace musik { namespace core { namespace audio {

    class ITransport {
        public:
            sigslot::signal2<int, std::string> StreamEvent;
            sigslot::signal1<int> PlaybackEvent;
            sigslot::signal0<> VolumeChanged;
            sigslot::signal1<double> TimeChanged;

            typedef enum {
                PlaybackStopped,
                PlaybackPaused,
                PlaybackPlaying
            } PlaybackState;

            typedef enum {
                StreamScheduled = 1,
                StreamPlaying = 2,
                StreamAlmostDone = 3,
                StreamFinished = 4,
                StreamStopped = 5,
                StreamError = -1
            } StreamEventType;

            virtual ~ITransport() { }

            virtual void PrepareNextTrack(const std::string& trackUrl) = 0;

            virtual void Start(const std::string& trackUrl) = 0;
            virtual void Stop() = 0;
            virtual bool Pause() = 0;
            virtual bool Resume() = 0;

            virtual double Position() = 0;
            virtual void SetPosition(double seconds) = 0;

            virtual double Volume() = 0;
            virtual void SetVolume(double volume) = 0;

            virtual bool IsMuted() = 0;
            virtual void SetMuted(bool muted) = 0;

            virtual PlaybackState GetPlaybackState() = 0;
    };

} } }
