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

/** @file PlaybackService.h
 *  @brief High-level playback controller for a track list.
 *  @details Wraps an ITransport plus a TrackList playlist and implements the SDK
 *      IPlaybackService interface. Handles play/next/previous, repeat and shuffle,
 *      playlist editing, seek, volume, and playback remotes, marshaling transport
 *      events onto a UI message queue thread. */

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

/** @namespace musik::core::audio
 *  @brief Audio pipeline types: buffers, players, transports and streams. */
namespace musik { namespace core { namespace audio {

    /** @brief Manages playback of a playlist through an ITransport.
     *  @details Keeps a TrackList playlist (and an unshuffled copy), tracks the
     *      current index, applies repeat/shuffle/volume, and forwards transport
     *      events to registered sigslots on the bound message queue's thread. Also
     *      broadcasts state to IPlaybackRemote plugins. */
    class PlaybackService :
        public musik::core::sdk::IPlaybackService,
        public musik::core::runtime::IMessageTarget,
        public sigslot::has_slots<>
    {
        public:
            /* our unique events */
            /** @brief Emitted when the current track changes. */
            sigslot::signal2<size_t, musik::core::TrackPtr> TrackChanged;
            /** @brief Emitted when the repeat mode changes. */
            sigslot::signal0<> ModeChanged;
            /** @brief Emitted when shuffle is toggled.
             *  @details Argument is true when shuffle is enabled. */
            sigslot::signal1<bool> Shuffled;
            /** @brief Emitted whenever the playlist is edited. */
            sigslot::signal0<> QueueEdited;

            /* copied from Transport, but will be automatically called on the
            specified MessageQueue's thread! */
            /** @brief Emitted when the playback state changes (on the queue thread). */
            sigslot::signal1<musik::core::sdk::PlaybackState> PlaybackStateChanged;
            /** @brief Emitted when the stream state changes (on the queue thread). */
            sigslot::signal1<musik::core::sdk::StreamState> StreamStateChanged;
            /** @brief Emitted when the volume changes (on the queue thread). */
            sigslot::signal0<> VolumeChanged;
            /** @brief Emitted periodically with the current position (on the queue thread). */
            sigslot::signal1<double> TimeChanged;

            /** @brief Creates a playback service.
             *  @param messageQueue Queue whose thread handles UI-facing events.
             *  @param library Library used for track lookups.
             *  @param transport The transport to control. */
            PlaybackService(
                musik::core::runtime::IMessageQueue& messageQueue,
                musik::core::ILibraryPtr library,
                std::shared_ptr<musik::core::audio::ITransport> transport);

            /** @brief Creates a playback service using a default transport.
             *  @param messageQueue Queue whose thread handles UI-facing events.
             *  @param library Library used for track lookups. */
            PlaybackService(
                musik::core::runtime::IMessageQueue& messageQueue,
                musik::core::ILibraryPtr library);

            virtual ~PlaybackService();

            /** @brief IMessageTarget handler.
             *  @param message The incoming runtime message. */
            void ProcessMessage(musik::core::runtime::IMessage &message) override;

            /* IPlaybackService */
            /** @brief Starts playing the track at the given playlist index.
             *  @param index Zero-based index into the playlist. */
            void Play(size_t index) override;
            /** @brief Advances to the next track.
             *  @return true if playback moved to another track. */
            bool Next() override;
            /** @brief Moves to the previous track (or restarts the current one).
             *  @return true if playback moved to another track. */
            bool Previous() override;
            /** @brief Stops playback. */
            void Stop()  override { transport->Stop(); }
            /** @return The current repeat mode. */
            musik::core::sdk::RepeatMode GetRepeatMode()  override { return this->repeatMode; }
            /** @brief Sets the repeat mode.
             *  @param mode The new repeat mode. */
            void SetRepeatMode(musik::core::sdk::RepeatMode mode) override;
            /** @brief Cycles to the next repeat mode. */
            void ToggleRepeatMode() override;
            /** @return The current playback state. */
            musik::core::sdk::PlaybackState GetPlaybackState() override;
            /** @return true if shuffle is enabled. */
            bool IsShuffled() override;
            /** @brief Toggles shuffle mode. */
            void ToggleShuffle() override;
            /** @return The current playlist index.
             *  @note May be out of range if the playlist was modified externally. */
            size_t GetIndex() noexcept override;
            /** @return The number of tracks in the playlist. */
            size_t Count() override;
            /** @return The current volume, in the range [0.0, 1.0]. */
            double GetVolume() override;
            /** @brief Sets the volume.
             *  @param vol The new volume, in the range [0.0, 1.0]. */
            void SetVolume(double vol) override;
            /** @brief Toggles between playing and paused. */
            void PauseOrResume() override;
            /** @return true if output is muted. */
            bool IsMuted() override;
            /** @brief Toggles the muted state. */
            void ToggleMute() override;
            /** @return The current playback position, in seconds. */
            double GetPosition() override;
            /** @brief Seeks to the given position.
             *  @param seconds The target position, in seconds. */
            void SetPosition(double seconds) override;
            /** @return The duration of the current track, in seconds. */
            double GetDuration() override;
            /** @return The track at the given playlist index.
             *  @param index Zero-based playlist index. */
            musik::core::sdk::ITrack* GetTrack(size_t index) override;
            /** @return The currently playing track, or nullptr. */
            musik::core::sdk::ITrack* GetPlayingTrack() override;
            /** @brief Replaces the playlist with the given track list.
             *  @param source The track list to copy from. */
            void CopyFrom(const musik::core::sdk::ITrackList* source) override;
            /** @brief Replaces the playlist and starts playing at the given index.
             *  @param source The track list to copy from.
             *  @param index Index of the track to play first. */
            void Play(const musik::core::sdk::ITrackList* source, size_t index) override;
            /** @return An editor for modifying the playlist. */
            musik::core::sdk::ITrackListEditor* EditPlaylist() override;
            /** @return The current time-change mode. */
            musik::core::sdk::TimeChangeMode GetTimeChangeMode() noexcept override;
            /** @brief Sets the time-change mode.
             *  @param mode The new time-change mode. */
            void SetTimeChangeMode(musik::core::sdk::TimeChangeMode) noexcept override;
            /** @brief Re-initializes the output device. */
            void ReloadOutput() override;
            /** @return A copy of the current playlist.
             *  @note Caller owns the returned object. */
            musik::core::sdk::ITrackList* Clone() override;

            /* TODO: include in SDK? */
            /** @brief Replaces the playlist from a concrete TrackList and plays.
             *  @param source Source track list.
             *  @param index Index to start playing at.
             *  @return true on success. */
            virtual bool HotSwap(const TrackList& source, size_t index = 0);

            /* app-specific implementation. similar to some SDK methods, but use
            concrete data types with known optimizations */
            /** @brief Plays a concrete track list from the given index.
             *  @param tracks The tracks to play.
             *  @param index Index to start at. */
            void Play(const musik::core::TrackList& tracks, size_t index);
            /** @brief Prepares the track at the given index for playback.
             *  @param index Playlist index to prepare.
             *  @param position Optional start position, in seconds. */
            void Prepare(size_t index, double position = 0.0f);
            /** @brief Copies the playlist into the given concrete track list.
             *  @param target Destination track list. */
            void CopyTo(musik::core::TrackList& target);
            /** @brief Replaces the playlist from a concrete track list.
             *  @param source Source track list. */
            void CopyFrom(const musik::core::TrackList& source);
            /** @return The currently playing track, or nullptr. */
            musik::core::TrackPtr GetPlaying();

            /** @return A reference to the underlying transport. */
            musik::core::audio::ITransport& GetTransport() noexcept {
                return *this->transport.get();
            }

            /** @return A non-owning shared view of the current playlist. */
            std::shared_ptr<const musik::core::TrackList> GetTrackList() noexcept {
                return std::shared_ptr<const musik::core::TrackList>(
                    &this->playlist, [](const musik::core::TrackList*) {});
            }

            /** @brief RAII editor for the playlist.
             *  @details Holds the playlist lock until destructed; all edits are
             *      applied to the internal TrackList and queued as messages. */
            class Editor : public musik::core::sdk::ITrackListEditor {
                public:
                    using IEditor = std::shared_ptr<musik::core::sdk::ITrackListEditor>; /**< Editor alias. */

                    Editor(Editor&& other);
                    virtual ~Editor();

                    /* ITrackListEditor */
                    /** @brief Inserts a track at the given index.
                     *  @param id The track id.
                     *  @param index Destination index.
                     *  @return true on success. */
                    bool Insert(int64_t id, size_t index) override;
                    /** @brief Swaps two playlist entries.
                     *  @param index1 First index.
                     *  @param index2 Second index.
                     *  @return true on success. */
                    bool Swap(size_t index1, size_t index2) override;
                    /** @brief Moves an entry from one index to another.
                     *  @param from Source index.
                     *  @param to Destination index.
                     *  @return true on success. */
                    bool Move(size_t from, size_t to) override;
                    /** @brief Deletes the entry at the given index.
                     *  @param index Index to delete.
                     *  @return true on success. */
                    bool Delete(size_t index) override;
                    /** @brief Appends a track to the playlist.
                     *  @param id The track id. */
                    void Add(const int64_t id) override;
                    /** @brief Empties the playlist. */
                    void Clear() override;
                    /** @brief Shuffles the playlist. */
                    void Shuffle() override;
                    /** @brief Releases the editor, committing queued changes.
                     *  @note Does not delete the editor; call delete after Release(). */
                    void Release() noexcept override;

                private:
                    friend class PlaybackService;
                    using Mutex = std::recursive_mutex; /**< Playlist mutex alias. */
                    using Lock = std::unique_lock<Mutex>; /**< RAII lock alias. */
                    using Queue = musik::core::runtime::IMessageQueue; /**< Queue alias. */

                    /** @brief Constructs an editor bound to a playback service.
                     *  @param playback Owning playback service.
                     *  @param tracks Target playlist.
                     *  @param queue Queue to post edit messages on.
                     *  @param mutex Playlist lock to hold. */
                    Editor(
                        PlaybackService& playback,
                        TrackList& tracks,
                        Queue& queue,
                        Mutex& mutex);

                    PlaybackService& playback; /**< Owning playback service. */
                    IEditor tracks;            /**< Underlying track list editor. */
                    Queue& queue;              /**< Message queue for edit notifications. */
                    Lock lock;                 /**< Held playlist lock. */
                    size_t playIndex;          /**< Current play index captured at edit time. */
                    bool nextTrackInvalidated; /**< Whether the next-track preload needs refreshing. */
                    bool edited;               /**< Whether the playlist was modified. */
            };

            /** @return An Editor for modifying the playlist. */
            Editor Edit();

        private:
            /** @brief Forwards stream events.
             *  @param eventType The stream state.
             *  @param uri The associated URI. */
            void OnStreamEvent(musik::core::sdk::StreamState eventType, std::string uri);
            /** @brief Forwards playback events.
             *  @param eventType The playback state. */
            void OnPlaybackEvent(musik::core::sdk::PlaybackState eventType);
            /** @brief Handles track changes.
             *  @param pos New playlist index.
             *  @param track The new track. */
            void OnTrackChanged(size_t pos, musik::core::TrackPtr track);
            /** @brief Handles volume changes. */
            void OnVolumeChanged();
            /** @brief Handles time changes.
             *  @param time The new position, in seconds. */
            void OnTimeChanged(double time);
            /** @brief Handles indexer completion.
             *  @param trackCount Number of indexed tracks. */
            void OnIndexerFinished(int trackCount);

            /** @brief Broadcasts the current mode to all remotes. */
            void NotifyRemotesModeChanged();
            /** @brief Preloads the next track in the queue. */
            void PrepareNextTrack();
            /** @brief Initializes playback remote plugins. */
            void InitRemotes();
            /** @brief Resets all playback remote plugins. */
            void ResetRemotes();
            /** @brief Marks a track as played in the library.
             *  @param trackId The track id. */
            void MarkTrackAsPlayed(int64_t trackId);

            /** @brief Starts playback at an index with a start mode.
             *  @param index Playlist index.
             *  @param mode StartMode::Immediate or Wait. */
            void PlayAt(size_t index, ITransport::StartMode mode);

            /** @brief Resolves a track with a bounded lookup timeout.
             *  @param index Playlist index.
             *  @return The resolved track, or nullptr. */
            musik::core::TrackPtr TrackAtIndexWithTimeout(size_t index);

            /** @brief Returns the URI of the track at the given index.
             *  @param index Playlist index. */
            std::string UriAtIndex(size_t index);
            /** @brief Returns the gain of the track at the given index.
             *  @param index Playlist index. */
            musik::core::audio::ITransport::Gain GainAtIndex(size_t index);

            musik::core::TrackList playlist;    /**< The current (possibly shuffled) playlist. */
            musik::core::TrackList unshuffled;  /**< Original ordering when shuffled. */
            std::recursive_mutex playlistMutex; /**< Guards playlist access. */

            std::vector<std::shared_ptr<musik::core::sdk::IPlaybackRemote>> remotes; /**< Playback remote plugins. */
            std::shared_ptr<musik::core::Preferences> playbackPrefs; /**< Playback preferences. */
            std::shared_ptr<musik::core::Preferences> appPrefs;     /**< Application preferences. */
            musik::core::TrackPtr playingTrack;                     /**< Currently playing track. */

            musik::core::ILibraryPtr library; /**< Library for track lookups. */
            std::shared_ptr<musik::core::audio::ITransport> transport; /**< Underlying transport. */
            size_t index, nextIndex;          /**< Current and next playlist indices. */

            musik::core::sdk::RepeatMode repeatMode;      /**< Current repeat mode. */
            musik::core::sdk::TimeChangeMode timeChangeMode; /**< Current time-change mode. */

            double seekPosition; /**< Pending seek position, in seconds. */

            musik::core::runtime::IMessageQueue& messageQueue; /**< UI message queue. */
    };

} } }
