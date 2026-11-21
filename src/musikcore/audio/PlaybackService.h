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


#include <musikcore/sdk/IPlaybackService.h>
#include <musikcore/sdk/IPlaybackRemote.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/track/TrackList.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/audio/MasterTransport.h>
#include <musikcore/support/Preferences.h>
#include <common/runtime/IMessageQueue.h>
#include <musikcore/playback.h>

#include <mutex>
#include <thread>

struct LibraryIdentity;

namespace musik { namespace core { namespace audio {

    class PlaybackService :
        public musik::core::sdk::IPlaybackService,
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
            Subscriptions<size_t, musik::core::TrackPtr> TrackChanged;
            Subscriptions<musik::core::sdk::PlaybackState> PlaybackStateChanged;
            Subscriptions<> QueueEdited;

            PlaybackService(
                musik::core::runtime::IMessageQueue& messageQueue,
                musik::core::ILibraryPtr library,
                std::shared_ptr<musik::core::audio::ITransport> transport);

            PlaybackService(
                musik::core::runtime::IMessageQueue& messageQueue,
                musik::core::ILibraryPtr library);

            virtual ~PlaybackService();

            /* finish local saved-state restoration before publishing a managed engine. */
            void Initialize(bool synchronousRestore = false);

            /* IMessageTarget */
            void ProcessMessage(musik::core::runtime::IMessage &message) override;

            /* IPlaybackService */
            void Play(size_t index) override;
            bool Next() override;
            bool Previous() override;
            void Shutdown();
            void Stop()  override { transport->Stop(); }
            musik::core::sdk::RepeatMode GetRepeatMode()  override { return this->repeatMode; }
            void SetRepeatMode(musik::core::sdk::RepeatMode mode) override;
            void ToggleRepeatMode() override;
            musik::core::sdk::PlaybackState GetPlaybackState() override;
            bool IsShuffled() override;
            void ToggleShuffle() override;
            size_t GetIndex() noexcept override;
            size_t Count() override;
            double GetVolume() override;
            void SetVolume(double vol) override;
            void PauseOrResume() override;
            bool IsMuted() override;
            void ToggleMute() override;
            double GetPosition() override;
            void SetPosition(double seconds) override;
            double GetDuration() override;
            musik::core::sdk::ITrack* GetTrack(size_t index) override;
            musik::core::sdk::ITrack* GetPlayingTrack() override;
            void CopyFrom(const musik::core::sdk::ITrackList* source) override;
            void Play(const musik::core::sdk::ITrackList* source, size_t index) override;
            musik::core::sdk::ITrackListEditor* EditPlaylist() override;
            musik::core::sdk::TimeChangeMode GetTimeChangeMode() noexcept override;
            void SetTimeChangeMode(musik::core::sdk::TimeChangeMode) noexcept override;
            void ReloadOutput() override;
            musik::core::sdk::ITrackList* Clone() override;

            /* TODO: include in SDK? */
            virtual bool HotSwap(const TrackList& source, size_t index = 0);

            /* app-specific implementation. similar to some SDK methods, but use
            concrete data types with known optimizations */
            void Play(const musik::core::TrackList& tracks, size_t index);
            void Prepare(size_t index, double position = 0.0f);
            void CopyTo(musik::core::TrackList& target);
            void CopyFrom(const musik::core::TrackList& source);
            musik::core::TrackPtr GetPlaying();
            void SelectLibrary(musik::core::ILibraryPtr library);
            void InvalidateDisconnectedCatalog();
            mc_result OpenHandle(mc_playback** out);

            musik::core::audio::ITransport& GetTransport() noexcept {
                return *this->transport.get();
            }

            /* required to make changes to the playlist. this little data structure
            privately owns a lock to the internal data structure and will release
            that lock when it's destructed. */
            class Editor : public musik::core::sdk::ITrackListEditor {
                public:
                    using IEditor = std::shared_ptr<musik::core::sdk::ITrackListEditor>;

                    Editor(Editor&& other);
                    virtual ~Editor();

                    /* ITrackListEditor */
                    bool Insert(int64_t id, size_t index) override;
                    bool Swap(size_t index1, size_t index2) override;
                    bool Move(size_t from, size_t to) override;
                    bool Delete(size_t index) override;
                    void Add(const int64_t id) override;
                    void Clear() override;
                    void Shuffle() override;
                    void Release() noexcept override;

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
                    IEditor tracks;
                    Queue& queue;
                    Lock lock;
                    size_t playIndex;
                    bool nextTrackInvalidated;
                    bool edited;
            };

            Editor Edit();

        private:
            void OnStreamEvent(musik::core::sdk::StreamState eventType, std::string uri);
            void OnPlaybackEvent(musik::core::sdk::PlaybackState eventType);
            void OnTrackChanged(size_t pos, musik::core::TrackPtr track);
            void OnVolumeChanged();
            void OnTimeChanged(double time);
            void OnIndexerFinished(int trackCount);

            void NotifyRemotesModeChanged();
            void PrepareNextTrack();
            void InitRemotes();
            void ResetRemotes();
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
            bool MatchesOrigin(const LibraryIdentity& origin) const;
            std::shared_ptr<const LibraryIdentity> CaptureOrigin(int libraryId) const;

            ITransport::Connections transportConnections;
            Subscriptions<int>::Connection indexFinished;
            lastfm::PlaybackReports lastFm;
            bool stopped = false;
            musik::core::TrackList playlist;
            uint64_t queueRevision = 0;
            std::shared_ptr<const LibraryIdentity> queueOrigin;
            std::shared_ptr<const LibraryIdentity> playingOrigin;
            std::shared_ptr<PlaybackEvents> apiEvents;
            const std::thread::id apiOwner = std::this_thread::get_id();
            musik::core::TrackList unshuffled;
            std::recursive_mutex playlistMutex;

            std::vector<std::shared_ptr<musik::core::sdk::IPlaybackRemote>> remotes;
            std::shared_ptr<musik::core::Preferences> playbackPrefs;
            std::shared_ptr<musik::core::Preferences> appPrefs;
            musik::core::TrackPtr playingTrack;

            musik::core::ILibraryPtr library;
            std::shared_ptr<musik::core::audio::ITransport> transport;
            size_t index, nextIndex;

            musik::core::sdk::RepeatMode repeatMode;
            musik::core::sdk::TimeChangeMode timeChangeMode;

            double seekPosition;

            musik::core::runtime::IMessageQueue& messageQueue;
    };

} } }
