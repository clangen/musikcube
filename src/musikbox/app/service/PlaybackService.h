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

#include <sigslot/sigslot.h>

#include <app/model/TrackList.h>

#include <cursespp/IMessageTarget.h>

#include <core/sdk/IPlaybackService.h>
#include <core/sdk/IPlaybackRemote.h>
#include <core/library/track/Track.h>
#include <core/library/ILibrary.h>
#include <core/audio/ITransport.h>

#include <boost/thread/recursive_mutex.hpp>

namespace musik {
    namespace box {
        class PlaybackService :
            public musik::core::sdk::IPlaybackService,
            public cursespp::IMessageTarget,
            public sigslot::has_slots<>
        {
            public:
                sigslot::signal2<size_t, musik::core::TrackPtr> TrackChanged;
                sigslot::signal0<> ModeChanged;
                sigslot::signal1<bool> Shuffled;

                PlaybackService(
                    musik::core::LibraryPtr library,
                    musik::core::audio::ITransport& transport);

                ~PlaybackService();

                /* IMessageTarget */
                virtual void ProcessMessage(cursespp::IMessage &message);

                /* IPlaybackService */
                virtual void Play(size_t index);
                virtual bool Next();
                virtual bool Previous();
                virtual void Stop() { transport.Stop(); }
                virtual RepeatMode GetRepeatMode() { return this->repeatMode; }
                virtual void SetRepeatMode(RepeatMode mode);
                virtual void ToggleRepeatMode();
                virtual bool IsShuffled();
                virtual void ToggleShuffle();
                virtual size_t GetIndex();
                virtual size_t Count();
                virtual double GetVolume();
                virtual void SetVolume(double vol);
                virtual void PauseOrResume();
                virtual bool IsMuted();
                virtual void ToggleMute();

                /* app-specific implementation */
                musik::core::audio::ITransport& GetTransport() { return this->transport; }
                void Play(TrackList& tracks, size_t index);
                void CopyTo(TrackList& target);
                musik::core::TrackPtr GetTrackAtIndex(size_t index);

            private:
                void OnStreamEvent(int eventType, std::string uri);
                void OnPlaybackEvent(int eventType);
                void OnTrackChanged(size_t pos, musik::core::TrackPtr track);
                void PrepareNextTrack();
                void InitRemotes();
                void ResetRemotes();

                TrackList playlist;
                TrackList unshuffled;
                boost::recursive_mutex playlistMutex;

                std::vector<std::shared_ptr<musik::core::sdk::IPlaybackRemote > > remotes;

                musik::core::LibraryPtr library;
                musik::core::audio::ITransport& transport;
                size_t index, nextIndex;
                RepeatMode repeatMode;
        };
    }
}
