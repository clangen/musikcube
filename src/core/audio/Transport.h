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

//////////////////////////////////////////////////////////////////////////////
namespace musik { namespace core { namespace audio {
//////////////////////////////////////////////////////////////////////////////

class Transport : public sigslot::has_slots<>{
    public:
        Transport();
        ~Transport();

        void PrepareNextTrack(utfstring trackUrl);
        void Start(utfstring trackUrl);
        void Stop();
        bool Pause();
        bool Resume();

        double Position();
        void SetPosition(double seconds);

        double Volume();
        void SetVolume(double volume);

    public:

        typedef enum {
            Started = 1,
            Ended   = 2,
            Error   = 3
        } PlaybackStatus;

        typedef sigslot::signal1<int> PlaybackStatusEvent;
        PlaybackStatusEvent PlaybackStatusChange;

        typedef sigslot::signal0<> PlaybackEvent;
        PlaybackEvent PlaybackAlmostDone;
//        PlaybackEvent PlaybackChange;
        PlaybackEvent PlaybackStarted;
        PlaybackEvent PlaybackEnded;
        PlaybackEvent PlaybackPause;
        PlaybackEvent PlaybackResume;
	PlaybackEvent PlaybackError;

    private:
        void OnPlaybackStarted(Player *player);
        void OnPlaybackAlmostEnded(Player *player);
        void OnPlaybackEnded(Player *player);
	void OnPlaybackError(Player *player);

    private:
        double volume;
        bool gapless;

        typedef std::list<PlayerPtr> PlayerList;
        PlayerList players;
        PlayerPtr currentPlayer;
        PlayerPtr nextPlayer;
};

//////////////////////////////////////////////////////////////////////////////
} } }
//////////////////////////////////////////////////////////////////////////////

