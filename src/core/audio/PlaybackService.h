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

#include <core/sdk/IPlaybackService.h>
#include <core/sdk/IPlaybackRemote.h>
#include <core/library/track/Track.h>
#include <core/library/track/TrackList.h>
#include <core/library/ILibrary.h>
#include <core/audio/ITransport.h>
#include <core/support/Preferences.h>
#include <core/runtime/IMessageQueue.h>

#include <mutex>

namespace musik { namespace core { namespace audio {

    class PlaybackService :
        public musik::core::sdk::IPlaybackService,
        public musik::core::runtime::IMessageTarget,
        public sigslot::has_slots<>
    {
        public:
            /* our unique events */
            sigslot::signal2<size_t, musik::core::TrackPtr> TrackChanged;
            sigslot::signal0<> ModeChanged;
            sigslot::signal1<bool> Shuffled;

            /* copied from Transport, but will be automatically called on the
            specified MessageQueue's thread! */
            sigslot::signal1<int> PlaybackEvent;
            sigslot::signal0<> VolumeChanged;
            sigslot::signal1<double> TimeChanged;

            PlaybackService(
                musik::core::runtime::IMessageQueue& messageQueue,
                musik::core::LibraryPtr library,
                musik::core::audio::ITransport& transport);

            ~PlaybackService();

            /* IMessageTarget */
            virtual void ProcessMessage(musik::core::runtime::IMessage &message);

            /* IPlaybackService */
            virtual void Play(size_t index);
            virtual bool Next();
            virtual bool Previous();
            virtual void Stop() { transport.Stop(); }
            virtual musik::core::sdk::RepeatMode GetRepeatMode() { return this->repeatMode; }
            virtual void SetRepeatMode(musik::core::sdk::RepeatMode mode);
            virtual void ToggleRepeatMode();
            virtual musik::core::sdk::PlaybackState GetPlaybackState();
            virtual bool IsShuffled();
            virtual void ToggleShuffle();
            virtual size_t GetIndex();
            virtual size_t Count();
            virtual double GetVolume();
            virtual void SetVolume(double vol);
            virtual void PauseOrResume();
            virtual bool IsMuted();
            virtual void ToggleMute();
            virtual double GetPosition();
            virtual void SetPosition(double seconds);
            virtual double GetDuration();
            virtual musik::core::sdk::IRetainedTrack* GetTrack(size_t index);

            /* app-specific implementation */
            musik::core::audio::ITransport& GetTransport() { return this->transport; }
            void Play(musik::core::TrackList& tracks, size_t index);
            void CopyTo(musik::core::TrackList& target);
            musik::core::TrackPtr GetTrackAtIndex(size_t index);

            /* required to make changes to the playlist. this little data structure
            privately owns a lock to the internal data structure and will release
            that lock when it's destructed. */
            class Editor {
                public:
                    using TrackListEditor = musik::core::sdk::ITrackListEditor;

                    Editor(Editor&& other);
                    ~Editor();

                    TrackListEditor& Tracks();

                private:
                    friend class PlaybackService;
                    using Mutex = std::recursive_mutex;
                    using Lock = std::unique_lock<Mutex>;
                    using Queue = musik::core::runtime::IMessageQueue;

                    Editor(TrackListEditor& tracks, Queue& queue, Mutex& mutex);

                    TrackListEditor& tracks;
                    Queue& queue;
                    Lock lock;
            };

            Editor Edit();

        private:
            void OnStreamEvent(int eventType, std::string uri);
            void OnPlaybackEvent(int eventType);
            void OnTrackChanged(size_t pos, musik::core::TrackPtr track);
            void OnVolumeChanged();
            void OnTimeChanged(double time);
            void PrepareNextTrack();
            void InitRemotes();
            void ResetRemotes();

            musik::core::TrackList playlist;
            musik::core::TrackList unshuffled;
            std::recursive_mutex playlistMutex;

            std::vector<std::shared_ptr<musik::core::sdk::IPlaybackRemote > > remotes;
            std::shared_ptr<musik::core::Preferences> prefs;

            musik::core::LibraryPtr library;
            musik::core::audio::ITransport& transport;
            size_t index, nextIndex;
            musik::core::sdk::RepeatMode repeatMode;

            musik::core::runtime::IMessageQueue& messageQueue;
    };

} } }
