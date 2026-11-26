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

#include <musikcore/support/LastFm.h>


#include <musikcore/library/track/Track.h>
#include <musikcore/library/track/TrackList.h>
#include <musikcore/library/LibraryBackend.h>
#include <musikcore/audio/MasterTransport.h>
#include <musikcore/support/Preferences.h>
#include <common/runtime/IMessageQueue.h>
#include <musikcore/playback.h>

#include <mutex>
#include <thread>

namespace musik { namespace core { namespace library { class Library; } } }

namespace musik { namespace core { namespace audio {

    class PlaybackService :
        public musik::core::runtime::IMessageTarget
    {
        enum MessageType {
            MESSAGE_STREAM_EVENT = 1000,
            MESSAGE_PLAYBACK_EVENT = 1001,
            MESSAGE_PREPARE_NEXT_TRACK = 1002,
            MESSAGE_VOLUME_CHANGED = 1003,
            MESSAGE_TIME_CHANGED = 1004,
            MESSAGE_MODE_CHANGED = 1005,
            MESSAGE_SHUFFLED = 1006,
            MESSAGE_NOTIFY_EDITED = 1007,
            MESSAGE_NOTIFY_RESET = 1008,
            MESSAGE_SEEK = 1009,
            MESSAGE_RELOAD_OUTPUT = 1010,
            MESSAGE_LOAD_PLAYBACK_CONTEXT = 1011,
            MESSAGE_MARK_TRACK_PLAYED = 1012,
            MESSAGE_NEXT_TRACK_EDITED = 1013,
            MESSAGE_MARK_TRACK_PLAYING = 1014,
            MESSAGE_NOTIFY_API_CHANGED = 1015,
        };

        class StreamMessage;

        public:
            /* Published on the service queue's dispatcher. */
            api::Subscriptions<size_t, musik::core::TrackPtr> TrackChanged;
            api::Subscriptions<musik::core::audio::PlaybackState> PlaybackStateChanged;
            api::Subscriptions<> QueueEdited;

            PlaybackService(
                musik::core::runtime::IMessageQueue& messageQueue,
                musik::core::LibraryBackend::Shared library,
                std::shared_ptr<musik::core::audio::ITransport> transport);

            PlaybackService(
                musik::core::runtime::IMessageQueue& messageQueue,
                musik::core::LibraryBackend::Shared library);

            virtual ~PlaybackService();

            /* finish local saved-state restoration before publishing a managed engine. */
            void Initialize(bool synchronousRestore = false);

            /* IMessageTarget */
            void ProcessMessage(musik::core::runtime::IMessage &message) override;

            /* playback commands and state. */
            void Play(size_t index);
            bool Next();
            bool Previous();
            void Shutdown();
            void Stop()  { transport->Stop(); }
            musik::core::audio::RepeatMode GetRepeatMode()  { return this->repeatMode; }
            void SetRepeatMode(musik::core::audio::RepeatMode mode);
            void ToggleRepeatMode();
            musik::core::audio::PlaybackState GetPlaybackState();
            bool IsShuffled();
            void ToggleShuffle();
            size_t GetIndex() noexcept;
            size_t Count();
            double GetVolume();
            void SetVolume(double vol);
            mc_result Pause();
            mc_result Resume();
            void PauseOrResume();
            bool IsMuted();
            void ToggleMute();
            double GetPosition();
            void SetPosition(double seconds);
            double GetDuration();
            musik::core::TrackPtr GetTrack(size_t index);
            musik::core::audio::TimeChangeMode GetTimeChangeMode() noexcept;
            void SetTimeChangeMode(musik::core::audio::TimeChangeMode) noexcept;
            void ReloadOutput();

            /* TODO: include in SDK? */
            virtual bool HotSwap(const TrackList& source, size_t index = 0);

            /* app-specific implementation. similar to some SDK methods, but use
            concrete data types with known optimizations */
            void Play(const musik::core::TrackList& tracks, size_t index);
            void Prepare(size_t index, double position = 0.0f);
            void CopyTo(musik::core::TrackList& target);
            void CopyFrom(const musik::core::TrackList& source);
            musik::core::TrackPtr GetPlaying();
            void SelectLibrary(musik::core::LibraryBackend::Shared library);
            void InvalidateDisconnectedCatalog();
            mc_result OpenHandle(mc_playback** out);

            musik::core::audio::ITransport& GetTransport() noexcept {
                return *this->transport.get();
            }

            /* required to make changes to the playlist. this little data structure
            privately owns a lock to the internal data structure and will release
            that lock when it's destructed. */
            class Editor {
                public:
                    Editor(Editor&& other);
                    ~Editor();

                    /* edit the locked queue. */
                    bool Insert(int64_t id, size_t index);
                    bool Swap(size_t index1, size_t index2);
                    bool Move(size_t from, size_t to);
                    bool Delete(size_t index);
                    void Add(const int64_t id);
                    void Clear();
                    void Shuffle();

                private:
                    friend class PlaybackService;
                    using Mutex = std::recursive_mutex;
                    using Lock = std::unique_lock<Mutex>;
                    using Queue = musik::core::runtime::IMessageQueue;

                    Editor(
                        PlaybackService& playback,
                        TrackList& tracks,
                        Queue& queue,
                        Mutex& mutex);

                    PlaybackService& playback;
                    TrackList& tracks;
                    Queue& queue;
                    Lock lock;
                    size_t playIndex;
                    bool nextTrackInvalidated;
                    bool edited;
            };

            Editor Edit();

        private:
            void OnStreamEvent(musik::core::audio::StreamState eventType, std::string uri);
            void OnPlaybackEvent(musik::core::audio::PlaybackState eventType);
            void OnTrackChanged(size_t pos, musik::core::TrackPtr track);
            void OnVolumeChanged();
            void OnTimeChanged(double time);
            void OnIndexerFinished(int trackCount);

            void PrepareNextTrack();
            void MarkTrackAsPlayed(int64_t trackId);

            void PlayAt(size_t index, ITransport::StartMode mode);

            musik::core::TrackPtr TrackAtIndexWithTimeout(size_t index);
            class PlaybackHandle;
            struct PlaybackEvents;

            std::string UriAtIndex(size_t index);
            musik::core::audio::ITransport::Gain GainAtIndex(size_t index);
            void ClosePlaybackHandles();
            static std::shared_ptr<PlaybackEvents> CreatePlaybackEvents();
            void NotifyPlaybackChanged(mc_playback_event event);
            void DeliverPlaybackNotification(runtime::IMessage& message);
            bool MatchesOrigin(const musik::core::library::Library& origin) const;
            std::shared_ptr<const musik::core::library::Library> CaptureOrigin(int libraryId) const;

            ITransport::Connections transportConnections;
            api::Subscriptions<int>::Connection indexFinished;
            api::Subscriptions<int>::Connection indexCancelled;
            lastfm::PlaybackReports lastFm;
            bool stopped = false;
            musik::core::TrackList playlist;
            uint64_t queueRevision = 0;
            std::shared_ptr<const musik::core::library::Library> queueOrigin;
            std::shared_ptr<const musik::core::library::Library> playingOrigin;
            std::shared_ptr<PlaybackEvents> apiEvents;
            const std::thread::id apiOwner = std::this_thread::get_id();
            musik::core::TrackList unshuffled;
            std::recursive_mutex playlistMutex;

            std::shared_ptr<musik::core::Preferences> playbackPrefs;
            std::shared_ptr<musik::core::Preferences> appPrefs;
            musik::core::TrackPtr playingTrack;

            musik::core::LibraryBackend::Shared library;
            std::shared_ptr<musik::core::audio::ITransport> transport;
            size_t index, nextIndex;
            ITransport::StartMode requestedStartMode { ITransport::StartMode::Immediate };

            musik::core::audio::RepeatMode repeatMode;
            musik::core::audio::TimeChangeMode timeChangeMode;

            double seekPosition;

            musik::core::runtime::IMessageQueue& messageQueue;
    };

} } }
