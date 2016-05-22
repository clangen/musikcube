//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Daniel Önnerby
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
#include <core/audio/Player.h>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <sigslot/sigslot.h>
#include <boost/thread/recursive_mutex.hpp>

namespace musik { namespace core { namespace audio {

    class Transport : public sigslot::has_slots<> {
        public:
            sigslot::signal2<int, std::string> PlaybackEvent;
            sigslot::signal0<> VolumeChanged;

            typedef enum {
                StateStopped,
                StatePaused,
                StatePlaying
            } PlaybackState;

            typedef enum {
                EventScheduled = 0,
                EventPlaying = 1,
                EventPaused = 2,
                EventResumed = 3,
                EventAlmostDone = 4,
                EventStopped = 5,
                EventError = -1
            } PlaybackEventType;

            Transport();
            ~Transport();

            void PrepareNextTrack(std::string trackUrl);
            void Start(std::string trackUrl);
            void Stop();
            bool Pause();
            bool Resume();

            double Duration();
            double Position();
            void SetPosition(double seconds);

            double Volume();
            void SetVolume(double volume);

            PlaybackState GetPlaybackState();

        private:
            void RaisePlaybackEvent(int type, PlayerPtr player);

            void OnPlaybackStarted(Player *player);
            void OnPlaybackAlmostEnded(Player *player);
            void OnPlaybackEnded(Player *player);
            void OnPlaybackError(Player *player);

        private:
            double volume;
            PlaybackState state;

            boost::mutex stateMutex;
            PlayerPtr currentPlayer;
            PlayerPtr nextPlayer;

    };

} } }
