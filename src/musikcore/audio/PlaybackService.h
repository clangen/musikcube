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

#pragma once

#include <sigslot/sigslot.h>

#include <musikcore/sdk/IPlaybackService.h>
#include <musikcore/sdk/IPlaybackRemote.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/track/TrackList.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/audio/MasterTransport.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/runtime/IMessageQueue.h>

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
            sigslot::signal0<> QueueEdited;

            /* copied from Transport, but will be automatically called on the
            specified MessageQueue's thread! */
            sigslot::signal1<musik::core::sdk::PlaybackState> PlaybackStateChanged;
            sigslot::signal1<musik::core::sdk::StreamState> StreamStateChanged;
            sigslot::signal0<> VolumeChanged;
            sigslot::signal1<double> TimeChanged;

            EXPORT PlaybackService(
                musik::core::runtime::IMessageQueue& messageQueue,
                musik::core::ILibraryPtr library,
                std::shared_ptr<musik::core::audio::ITransport> transport);

            EXPORT PlaybackService(
                musik::core::runtime::IMessageQueue& messageQueue,
                musik::core::ILibraryPtr library);

            EXPORT virtual ~PlaybackService();

            /* IMessageTarget */
            EXPORT void ProcessMessage(musik::core::runtime::IMessage &message) override;

            /* IPlaybackService */
            EXPORT void Play(size_t index) override;
            EXPORT bool Next() override;
            EXPORT bool Previous() override;
            EXPORT void Stop()  override { transport->Stop(); }
            EXPORT musik::core::sdk::RepeatMode GetRepeatMode()  override { return this->repeatMode; }
            EXPORT void SetRepeatMode(musik::core::sdk::RepeatMode mode) override;
            EXPORT void ToggleRepeatMode() override;
            EXPORT musik::core::sdk::PlaybackState GetPlaybackState() override;
            EXPORT bool IsShuffled() override;
            EXPORT void ToggleShuffle() override;
            EXPORT size_t GetIndex() noexcept override;
            EXPORT size_t Count() override;
            EXPORT double GetVolume() override;
            EXPORT void SetVolume(double vol) override;
            EXPORT void PauseOrResume() override;
            EXPORT bool IsMuted() override;
            EXPORT void ToggleMute() override;
            EXPORT double GetPosition() override;
            EXPORT void SetPosition(double seconds) override;
            EXPORT double GetDuration() override;
            EXPORT musik::core::sdk::ITrack* GetTrack(size_t index) override;
            EXPORT musik::core::sdk::ITrack* GetPlayingTrack() override;
            EXPORT void CopyFrom(const musik::core::sdk::ITrackList* source) override;
            EXPORT void Play(const musik::core::sdk::ITrackList* source, size_t index) override;
            EXPORT musik::core::sdk::ITrackListEditor* EditPlaylist() override;
            EXPORT musik::core::sdk::TimeChangeMode GetTimeChangeMode() noexcept override;
            EXPORT void SetTimeChangeMode(musik::core::sdk::TimeChangeMode) noexcept override;
            EXPORT void ReloadOutput() override;
            EXPORT musik::core::sdk::ITrackList* Clone() override;

            /* TODO: include in SDK? */
            EXPORT virtual bool HotSwap(const TrackList& source, size_t index = 0);

            /* app-specific implementation. similar to some SDK methods, but use
            concrete data types with known optimizations */
            EXPORT void Play(const musik::core::TrackList& tracks, size_t index);
            EXPORT void Prepare(size_t index, double position = 0.0f);
            EXPORT void CopyTo(musik::core::TrackList& target);
            EXPORT void CopyFrom(const musik::core::TrackList& source);
            EXPORT musik::core::TrackPtr GetPlaying();

            EXPORT musik::core::audio::ITransport& GetTransport() noexcept {
                return *this->transport.get();
            }

            EXPORT std::shared_ptr<const musik::core::TrackList> GetTrackList() noexcept {
                return std::shared_ptr<const musik::core::TrackList>(
                    &this->playlist, [](const musik::core::TrackList*) {});
            }

            /* required to make changes to the playlist. this little data structure
            privately owns a lock to the internal data structure and will release
            that lock when it's destructed. */
            class Editor : public musik::core::sdk::ITrackListEditor {
                public:
                    using IEditor = std::shared_ptr<musik::core::sdk::ITrackListEditor>;

                    EXPORT Editor(Editor&& other);
                    EXPORT virtual ~Editor();

                    /* ITrackListEditor */
                    EXPORT bool Insert(int64_t id, size_t index) override;
                    EXPORT bool Swap(size_t index1, size_t index2) override;
                    EXPORT bool Move(size_t from, size_t to) override;
                    EXPORT bool Delete(size_t index) override;
                    EXPORT void Add(const int64_t id) override;
                    EXPORT void Clear() override;
                    EXPORT void Shuffle() override;
                    EXPORT void Release() noexcept override;

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

            EXPORT Editor Edit();

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

            std::string UriAtIndex(size_t index);
            musik::core::audio::ITransport::Gain GainAtIndex(size_t index);

            musik::core::TrackList playlist;
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
