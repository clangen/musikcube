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

#include <pch.hpp>

#include "PlaybackService.h"

#include <musikcore/audio/MasterTransport.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/query/MarkTrackPlayedQuery.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/runtime/MessageQueue.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Playback.h>
#include <musikcore/support/LastFm.h>

using namespace musik::core::library;
using namespace musik::core;
using namespace musik::core::prefs;
using namespace musik::core::sdk;
using namespace musik::core::runtime;
using namespace musik::core::audio;
using namespace musik::core::library::query;

using musik::core::TrackPtr;
using musik::core::ILibraryPtr;
using musik::core::audio::ITransport;
using Editor = PlaybackService::Editor;

#define NO_POSITION (size_t) -1
#define START_OVER (size_t) -2

#define PREVIOUS_GRACE_PERIOD 2.0f

#define MESSAGE_STREAM_EVENT 1000
#define MESSAGE_PLAYBACK_EVENT 1001
#define MESSAGE_PREPARE_NEXT_TRACK 1002
#define MESSAGE_VOLUME_CHANGED 1003
#define MESSAGE_TIME_CHANGED 1004
#define MESSAGE_MODE_CHANGED 1005
#define MESSAGE_SHUFFLED 1006
#define MESSAGE_NOTIFY_EDITED 1007
#define MESSAGE_NOTIFY_RESET 1008
#define MESSAGE_SEEK 1009
#define MESSAGE_RELOAD_OUTPUT 1010
#define MESSAGE_LOAD_PLAYBACK_CONTEXT 1011
#define MESSAGE_MARK_TRACK_PLAYED 1012

class StreamMessage: public Message {
    public:
        StreamMessage(IMessageTarget* target, int eventType, const std::string& uri)
        : Message(target, MESSAGE_STREAM_EVENT, eventType, 0) {
            this->uri = uri;
        }

        std::string GetUri() {
            return this->uri;
        }

        int GetEventType() {
            return static_cast<int>(this->UserData1());
        }

    private:
        std::string uri;
};

#define POST(instance, type, user1, user2) \
    this->messageQueue.Post( \
        musik::core::runtime::Message::Create(instance, type, user1, user2));

#define POST_DELAYED(instance, type, user1, user2, afterMs) \
    this->messageQueue.Post( \
        musik::core::runtime::Message::Create(instance, type, user1, user2), afterMs);

#define POST_STREAM_MESSAGE(instance, eventType, uri) \
    this->messageQueue.Post( \
        musik::core::runtime::IMessagePtr(new StreamMessage(instance, eventType, uri)));

static inline void loadPreferences(ITransport& transport, IPlaybackService& playback, Preferences& prefs) {
    double volume = prefs.GetDouble(keys::Volume, 1.0f);
    volume = std::max(0.0f, std::min(1.0f, (float)volume));
    transport.SetVolume(volume);

    auto repeatMode = static_cast<RepeatMode>(
        prefs.GetInt(keys::RepeatMode, static_cast<int>(RepeatMode::None)));

    repeatMode = (repeatMode > RepeatMode::List || repeatMode < RepeatMode::None) ? RepeatMode::None : repeatMode;

    playback.SetRepeatMode(repeatMode);

    const auto timeChangeMode = static_cast<TimeChangeMode>(
        prefs.GetInt(keys::TimeChangeMode, static_cast<int>(TimeChangeMode::Scrub)));

    playback.SetTimeChangeMode(timeChangeMode);
}

static inline void savePreferences(IPlaybackService& playback, Preferences& prefs) {
    prefs.SetDouble(keys::Volume, playback.GetVolume());
    prefs.SetInt(keys::RepeatMode, static_cast<int>(playback.GetRepeatMode()));
    prefs.SetInt(keys::TimeChangeMode, static_cast<int>(playback.GetTimeChangeMode()));
}

/* internally PlaybackService leverages a message queue for synchronization;
tracks are a special in that they are heavy-weight so aggressively exjected
from caches... sometimes we may have to query for them. if they take more than
the specified timeout we consider it a failure and stop playback. */
#define TRACK_TIMEOUT_MS() (size_t) this->appPrefs->GetInt(keys::PlaybackTrackQueryTimeoutMs, 5000)

PlaybackService::PlaybackService(
    IMessageQueue& messageQueue,
    ILibraryPtr library,
    std::shared_ptr<ITransport> transport)
: library(library)
, transport(transport)
, playlist(library)
, unshuffled(library)
, repeatMode(RepeatMode::None)
, messageQueue(messageQueue)
, timeChangeMode(TimeChangeMode::Seek)
, seekPosition(-1.0f)
, index(NO_POSITION)
, nextIndex(NO_POSITION)
, playbackPrefs(Preferences::ForComponent(components::Playback))
, appPrefs(Preferences::ForComponent(components::Settings)) {
    transport->StreamEvent.connect(this, &PlaybackService::OnStreamEvent);
    transport->PlaybackEvent.connect(this, &PlaybackService::OnPlaybackEvent);
    transport->VolumeChanged.connect(this, &PlaybackService::OnVolumeChanged);
    transport->TimeChanged.connect(this, &PlaybackService::OnTimeChanged);
    library->Indexer()->Finished.connect(this, &PlaybackService::OnIndexerFinished);
    messageQueue.Register(this);
    messageQueue.Post(Message::Create(this, MESSAGE_LOAD_PLAYBACK_CONTEXT));
}

PlaybackService::PlaybackService(
    IMessageQueue& messageQueue,
    ILibraryPtr library)
: PlaybackService(messageQueue, library, std::make_shared<MasterTransport>()) {
    messageQueue.Register(this);
}

PlaybackService::~PlaybackService() {
    playback::SavePlaybackContext(library, *this);
    this->messageQueue.Unregister(this);
    savePreferences(*this, *playbackPrefs);
    this->Stop();
    this->ResetRemotes();
}

void PlaybackService::InitRemotes() {
    typedef PluginFactory::ReleaseDeleter<IPlaybackRemote> Deleter;

    this->remotes = PluginFactory::Instance()
        .QueryInterface<IPlaybackRemote, Deleter>("GetPlaybackRemote");

    for (auto it : remotes) {
        it->SetPlaybackService(this);
    }
}

void PlaybackService::ResetRemotes() {
    for (auto it : remotes) {
        it->SetPlaybackService(nullptr);
    }
}

void PlaybackService::PrepareNextTrack() {
    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);

    if (this->Count() > 0) {
        /* repeat track, just keep playing the same thing over and over */
        if (this->repeatMode == RepeatMode::Track) {
            this->nextIndex = this->index;
            this->transport->PrepareNextTrack(
                this->UriAtIndex(this->index),
                this->GainAtIndex(this->index));
        }
        else {
            /* annoying and confusing special case -- the user edited the
            playlist and deleted the currently playing item. let's start
            again from the top... */
            if (this->index == START_OVER) {
                if (this->playlist.Count() > 0) {
                    this->index = NO_POSITION;
                    this->nextIndex = 0;
                    this->transport->PrepareNextTrack(
                        this->UriAtIndex(nextIndex),
                        this->GainAtIndex(nextIndex));
                }
            }
            /* normal case, just move forward */
            else if (this->playlist.Count() > this->index + 1) {
                if (this->nextIndex != this->index + 1) {
                    this->nextIndex = this->index + 1;
                    this->transport->PrepareNextTrack(
                        this->UriAtIndex(nextIndex),
                        this->GainAtIndex(nextIndex));
                }
            }
            /* repeat list case, wrap around to the beginning if necessary */
            else if (this->repeatMode == RepeatMode::List) {
                if (this->nextIndex != 0) {
                    this->nextIndex = 0;
                    this->transport->PrepareNextTrack(
                        this->UriAtIndex(nextIndex),
                        this->GainAtIndex(nextIndex));
                }
            }
            else {
                /* nothing to prepare if we get here. */
                this->transport->PrepareNextTrack("", ITransport::Gain());
            }
        }
    }
}

void PlaybackService::SetRepeatMode(RepeatMode mode) {
    if (this->repeatMode != mode) {
        this->repeatMode = mode;
        POST(this, MESSAGE_PREPARE_NEXT_TRACK, NO_POSITION, 0);
        POST(this, MESSAGE_MODE_CHANGED, 0, 0);
    }
}

musik::core::sdk::TimeChangeMode PlaybackService::GetTimeChangeMode() noexcept {
    return this->timeChangeMode;
}

void PlaybackService::SetTimeChangeMode(TimeChangeMode mode) noexcept {
    this->timeChangeMode = mode;
}

void PlaybackService::ToggleShuffle() {
    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);

    /* remember the ID of the playing track -- we're going to need to look
    it up after the shuffle */
    int64_t id = -1;
    if (this->index < this->playlist.Count()) {
        id = this->playlist.GetId(this->index);
    }

    this->playlist.ClearCache();
    this->unshuffled.ClearCache();
    bool shuffled = false;

    if (this->unshuffled.Count() > 0) { /* shuffled -> unshuffled */
        this->playlist.Clear();
        this->playlist.Swap(this->unshuffled);
    }
    else { /* unshuffled -> shuffle */
        this->unshuffled.CopyFrom(this->playlist);
        this->playlist.Shuffle();
        shuffled = true;
    }

    /* find the new playback index and prefetch the next track */
    if (id != -1) {
        const int index = this->playlist.IndexOf(id);
        if (index != -1) {
            this->index = index;
            POST(this, MESSAGE_PREPARE_NEXT_TRACK, NO_POSITION, 0);
        }
    }

    POST(this, MESSAGE_SHUFFLED, shuffled ? 1 : 0, 0);
    POST(this, MESSAGE_NOTIFY_EDITED, 0, 0);
}

void PlaybackService::ProcessMessage(IMessage &message) {
    const int type = message.Type();
    if (type == MESSAGE_LOAD_PLAYBACK_CONTEXT) {
        loadPreferences(*this->transport, *this, *playbackPrefs);
        playback::LoadPlaybackContext(library, *this);
        this->InitRemotes();
    }
    else if (type == MESSAGE_MARK_TRACK_PLAYED) {
        lastfm::Scrobble(this->playingTrack);
        this->MarkTrackAsPlayed(message.UserData1()); /* UserData1 is a trackId */
    }
    else if (type == MESSAGE_STREAM_EVENT) {
        StreamMessage* streamMessage = dynamic_cast<StreamMessage*>(&message);
        const StreamState eventType = static_cast<StreamState>(streamMessage->GetEventType());

        if (eventType == StreamState::Buffering || eventType == StreamState::Buffered || eventType == StreamState::Playing) {
            TrackPtr track;

            {
                std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);
                if (this->nextIndex != NO_POSITION) {
                    /* in most cases when we get here it means that the next track is
                    starting, so we want to update our internal index. however, because
                    things are asynchronous, this may not always be the case, especially if
                    the tracks are very short, or the user is advancing through songs very
                    quickly. compare the track URIs before we update internal state. */
                    if (this->nextIndex >= this->Count()) {
                        this->nextIndex = NO_POSITION;
                        this->transport->PrepareNextTrack("", ITransport::Gain());
                        return;
                    }

                    if (this->UriAtIndex(this->nextIndex) == streamMessage->GetUri()) {
                        this->index = this->nextIndex;
                        this->nextIndex = NO_POSITION;
                    }
                }

                if (this->index != NO_POSITION) {
                    track = TrackAtIndexWithTimeout(this->index);
                }
            }

            if (track) {
                this->OnTrackChanged(this->index, track);
            }
            else {
                this->Stop();
                return;
            }

            if (eventType == StreamState::Playing) {
                this->PrepareNextTrack();
            }
        }

        bool raiseStreamEvent = false;
        {
            std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);
            raiseStreamEvent = this->UriAtIndex(this->index) == streamMessage->GetUri();
        }

        if (raiseStreamEvent) {
            this->StreamStateChanged(eventType);
        }
    }
    else if (type == MESSAGE_PLAYBACK_EVENT) {
        const PlaybackState eventType = static_cast<PlaybackState>(message.UserData1());

        if (eventType == PlaybackState::Stopped) {
            this->OnTrackChanged(NO_POSITION, TrackPtr());
        }
        else if (eventType == PlaybackState::Prepared) {
            /* notify track change as soon as we're prepared. if we wait until
            we start playing, it may be a while until the UI knows to redraw! */
            if (this->UriAtIndex(this->index) == transport->Uri()) {
                auto track = TrackAtIndexWithTimeout(this->index);
                if (track) {
                    this->OnTrackChanged(this->index, track);
                }
                else {
                    this->Stop();
                }
            }
        }

        for (auto it : remotes) {
            it->OnPlaybackStateChanged(static_cast<PlaybackState>(eventType));
        }

        this->PlaybackStateChanged(static_cast<PlaybackState>(eventType));
    }
    else if (type == MESSAGE_PREPARE_NEXT_TRACK) {
        if (transport->GetPlaybackState() != PlaybackState::Stopped) {
            const size_t updatedIndex = static_cast<size_t>(message.UserData1());

            if (updatedIndex != NO_POSITION) {
                this->index = updatedIndex;
                this->nextIndex = NO_POSITION; /* force recalc */
            }

            this->PrepareNextTrack();
        }
    }
    else if (type == MESSAGE_VOLUME_CHANGED) {
        const double volume = transport->Volume();
        for (auto it : remotes) {
            it->OnVolumeChanged(volume);
        }
        this->VolumeChanged();
    }
    else if (type == MESSAGE_MODE_CHANGED) {
        this->NotifyRemotesModeChanged();
        this->ModeChanged();
    }
    else if (type == MESSAGE_SHUFFLED) {
        this->NotifyRemotesModeChanged();
        this->Shuffled(!!message.UserData1());
    }
    else if (type == MESSAGE_TIME_CHANGED) {
        this->TimeChanged(transport->Position());
        const double volume = transport->Volume();
        for (auto remote : this->remotes) {
            remote->OnPlaybackTimeChanged(transport->Position());
        }
    }
    else if (type == MESSAGE_NOTIFY_EDITED ||
             type == MESSAGE_NOTIFY_RESET)
    {
        for (auto it : remotes) {
            it->OnPlayQueueChanged();
        }

        this->QueueEdited();
    }
    else if (type == MESSAGE_SEEK) {
        if (this->seekPosition != -1.0f) {
            this->transport->SetPosition(this->seekPosition + 0.5f);
            this->seekPosition = -1.0f;
        }
    }
    else if (type == MESSAGE_RELOAD_OUTPUT) {
        const auto state = this->GetPlaybackState();
        const auto index = this->GetIndex();
        const double time = this->GetPosition();

        /* we generally have a MasterTransport available, but apps may
        choose to implement their own transport and use it instead, so
        do this nasty check first... */
        MasterTransport* masterTransport =
            dynamic_cast<MasterTransport*>(this->transport.get());

        if (masterTransport) {
            const TransportType transportType = static_cast<TransportType>(
                playbackPrefs->GetInt(
                    keys::Transport,
                    static_cast<int>(TransportType::Gapless)));

            if (masterTransport->GetType() != transportType) {
                masterTransport->SwitchTo(transportType);
            }
        }

        if (state != PlaybackState::Stopped) {
            this->Stop();
            this->transport->ReloadOutput();

            if (index != NO_POSITION) {
                const auto startMode =
                    (state != PlaybackState::Paused && state != PlaybackState::Prepared)
                        ? ITransport::StartMode::Immediate
                        : ITransport::StartMode::Wait;

                this->PlayAt(index, startMode);
                if (time > 0.0f) {
                    this->transport->SetPosition(time);
                }

                if (startMode == ITransport::StartMode::Wait) {
                    this->transport->Pause();
                }
            }
        }
        else {
            this->transport->ReloadOutput();
        }
    }
}

void PlaybackService::NotifyRemotesModeChanged() {
    const RepeatMode mode = this->repeatMode;
    const bool shuffled = this->IsShuffled();
    for (auto it : remotes) {
        it->OnModeChanged(repeatMode, shuffled);
    }
}

void PlaybackService::OnTrackChanged(size_t pos, TrackPtr track) {
    this->playingTrack = track;
    this->TrackChanged(this->index, track);
    this->messageQueue.Remove(this, MESSAGE_MARK_TRACK_PLAYED);

    if (track && this->transport->GetStreamState() == StreamState::Playing) {
        /* we consider a track to be played if (1) it enters the playing state and
        it's less than 10 seconds long, or (2) it enters the playing state, and
        remains playing for > 25% of its duration seconds */
        const double duration = this->transport->GetDuration();
        if (duration > 0 && duration < 10.0) {
            lastfm::Scrobble(track);
            this->MarkTrackAsPlayed(track->GetId());
        }
        else {
            const int64_t delay = (int64_t)(duration * 0.25f) * 1000LL;
            POST_DELAYED(this, MESSAGE_MARK_TRACK_PLAYED, track->GetId(), 0, delay);
        }
    }

    for (auto it : remotes) {
        it->OnTrackChanged(track.get());
    }
}

void PlaybackService::MarkTrackAsPlayed(int64_t trackId) {
    this->library->Enqueue(std::make_shared<MarkTrackPlayedQuery>(trackId));
}

bool PlaybackService::Next() {
    if (transport->GetPlaybackState() == PlaybackState::Stopped) {
        return false;
    }

    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);

    if (this->playlist.Count() > index + 1) {
        this->Play(index + 1);
        return true;
    }
    else if (this->repeatMode == RepeatMode::List) {
        this->Play(0); /* wrap around */
        return true;
    }

    return false;
}

bool PlaybackService::Previous() {
    if (transport->GetPlaybackState() == PlaybackState::Stopped) {
        return false;
    }

    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);

    if (transport->Position() > PREVIOUS_GRACE_PERIOD) {
        this->Play(index);
        return true;
    }

    if (index > 0) {
        this->Play(index - 1);
        return true;
    }
    else if (this->repeatMode == RepeatMode::List) {
        this->Play(this->Count() - 1); /* wrap around */
        return true;
    }

    return false;
}

bool PlaybackService::IsShuffled() {
    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);
    return this->unshuffled.Count() > 0;
}

size_t PlaybackService::Count() {
    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);
    return this->playlist.Count();
}

void PlaybackService::ToggleRepeatMode() {
    const RepeatMode mode = GetRepeatMode();
    switch (mode) {
        case RepeatMode::None: SetRepeatMode(RepeatMode::List); break;
        case RepeatMode::List: SetRepeatMode(RepeatMode::Track); break;
        default: SetRepeatMode(RepeatMode::None); break;
    }
}

PlaybackState PlaybackService::GetPlaybackState() {
    return transport->GetPlaybackState();
}

bool PlaybackService::HotSwap(const TrackList& tracks, size_t index) {
    if (&tracks == &playlist) {
        return true;
    }

    if (!tracks.Count()) {
        return false;
    }

    bool found = false;
    auto playingTrack = this->GetPlaying();
    if (playingTrack && tracks.Count() > index) {
        const auto supplantId = tracks.GetId(index);
        const auto playingId = playingTrack->GetId();

        /* look at the index hint, see if we can find a matching track without
        iteration. */
        if (supplantId == playingId) {
            found = true;
        }
        /* otherwise search the input */
        else {
            for (size_t i = 0; i < tracks.Count(); i++) {
                if (tracks.GetId(i) == playingId) {
                    index = i;
                    found = true;
                }
            }
        }
    }

    {
        std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);
        TrackList temp(this->library);
        temp.CopyFrom(tracks);
        this->playlist.Swap(temp);
        this->unshuffled.Clear();
        this->index = found ? index : NO_POSITION;
        this->nextIndex = NO_POSITION;
    }

    if (found) {
        POST(this, MESSAGE_PREPARE_NEXT_TRACK, this->index, 0);
    }

    POST(this, MESSAGE_NOTIFY_EDITED, NO_POSITION, 0);

    return true;
}

void PlaybackService::Play(const TrackList& tracks, size_t index) {
    {
        std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);

        if (&tracks != &playlist) {
            TrackList temp(this->library);
            temp.CopyFrom(tracks);
            this->playlist.Swap(temp);
            this->unshuffled.Clear();
        }
    }

    if (index <= tracks.Count()) {
        this->Play(index);
    }

    POST(this, MESSAGE_NOTIFY_RESET, 0, 0);
}

void PlaybackService::Play(const musik::core::sdk::ITrackList* source, size_t index) {
    if (source) {
        /* see if we have a TrackList -- if we do we can optimize the copy */
        const TrackList* sourceTrackList = dynamic_cast<const TrackList*>(source);

        if (sourceTrackList) {
            this->Play(*sourceTrackList, index);
            return;
        }

        /* otherwise use slower impl to be compatible with SDK */
        {
            std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);
            if (source != &playlist) {
                this->CopyFrom(source);
                this->unshuffled.Clear();
            }
        }

        if (index <= source->Count()) {
            this->Play(index);
        }

        POST(this, MESSAGE_NOTIFY_RESET, 0, 0);
    }
}

void PlaybackService::ReloadOutput() {
    messageQueue.Debounce(Message::Create(this, MESSAGE_RELOAD_OUTPUT), 500);
}

void PlaybackService::CopyTo(TrackList& target) {
    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);
    target.CopyFrom(this->playlist);
}

void PlaybackService::CopyFrom(const TrackList& source) {
    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);

    this->playlist.CopyFrom(source);
    this->index = NO_POSITION;
    this->nextIndex = NO_POSITION;

    if (this->playingTrack) {
        this->index = playlist.IndexOf(this->playingTrack->GetId());
        POST(this, MESSAGE_PREPARE_NEXT_TRACK, this->index, 0);
    }

    POST(this, MESSAGE_NOTIFY_EDITED, NO_POSITION, 0);
}

void PlaybackService::CopyFrom(const musik::core::sdk::ITrackList* source) {
    if (source) {
        /* see if we have a TrackList -- if we do we can optimize the copy */
        const TrackList* sourceTrackList = dynamic_cast<const TrackList*>(source);

        if (sourceTrackList) {
            this->CopyFrom(*sourceTrackList);
            return;
        }

        /* otherwise we gotta do it one at a time */
        std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);

        this->playlist.Clear();
        for (size_t i = 0; i < source->Count(); i++) {
            this->playlist.Add(source->GetId(i));
        }

        this->index = NO_POSITION;
        this->nextIndex = NO_POSITION;

        if (this->playingTrack) {
            this->index = playlist.IndexOf(this->playingTrack->GetId());
            POST(this, MESSAGE_PREPARE_NEXT_TRACK, NO_POSITION, 0);
        }

        POST(this, MESSAGE_NOTIFY_EDITED, NO_POSITION, 0);
    }
}

void PlaybackService::PlayAt(size_t index, ITransport::StartMode mode) {
    index = std::min(this->Count(), index);

    std::string uri = this->UriAtIndex(index);
    const auto gain = this->GainAtIndex(index);

    if (uri.size()) {
        transport->Start(uri, gain, mode);
        this->nextIndex = NO_POSITION;
        this->index = index;
    }
}

void PlaybackService::Play(size_t index) {
    this->PlayAt(index, ITransport::StartMode::Immediate);
}

void PlaybackService::Prepare(size_t index, double position) {
    this->PlayAt(index, ITransport::StartMode::Wait);
    if (position > 0.0) {
        this->transport->SetPosition(position);
    }
}

size_t PlaybackService::GetIndex() noexcept {
    return this->index;
}

double PlaybackService::GetVolume() {
    return transport->Volume();
}

void PlaybackService::PauseOrResume() {
    const auto state = transport->GetPlaybackState();
    if (state == PlaybackState::Stopped) {
        if (this->Count()) {
            this->Play(0);
        }
    }
    else if (state == PlaybackState::Paused || state == PlaybackState::Prepared) {
        transport->Resume();
    }
    else if (state == PlaybackState::Playing) {
        transport->Pause();
    }
}

bool PlaybackService::IsMuted() {
    return transport->IsMuted();
}

void PlaybackService::ToggleMute() {
    transport->SetMuted(!transport->IsMuted());
}

void PlaybackService::SetVolume(double vol) {
    transport->SetVolume(vol);
}

double PlaybackService::GetPosition() {
    if (this->timeChangeMode == TimeChangeMode::Seek && this->seekPosition != -1.0f) {
        return this->seekPosition;
    }

    return transport->Position();
}

void PlaybackService::SetPosition(double seconds) {
    if (this->timeChangeMode == TimeChangeMode::Seek) {
        seconds = std::max(seconds, (double) 0.0);
        this->seekPosition = seconds;
        this->TimeChanged(seconds);
        messageQueue.Debounce(Message::Create(this, MESSAGE_SEEK), 500);
    }
    else { /* TimeChangeMode::Scrub */
        transport->SetPosition(seconds);
    }
}

double PlaybackService::GetDuration() {
    TrackPtr track;

    const double duration = this->transport->GetDuration();

    if (duration > 0) {
        return duration;
    }

    {
        std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);
        const size_t index = this->index;
        if (index < this->playlist.Count()) {
            track = TrackAtIndexWithTimeout(index);
        }
    }

    if (track) {
        std::stod(track->GetString(constants::Track::DURATION));
    }

    return 0.0f;
}

ITrack* PlaybackService::GetTrack(size_t index) {
    if (this->library->GetConnectionState() != ILibrary::ConnectionState::Connected) {
        return nullptr;
    }

    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);

    const size_t count = this->playlist.Count();

    if (count && index < this->playlist.Count()) {
        auto track = this->playlist.GetWithTimeout(index, TRACK_TIMEOUT_MS() * 10);
        if (track) {
            return track->GetSdkValue();
        }
    }

    return nullptr;
}

ITrack* PlaybackService::GetPlayingTrack() {
    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);

    if (this->playingTrack) {
        return this->playingTrack->GetSdkValue();
    }

    return nullptr;
}

TrackPtr PlaybackService::GetPlaying() {
    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);
    return this->playingTrack;
}

TrackPtr PlaybackService::TrackAtIndexWithTimeout(size_t index) {
    if (this->library->GetConnectionState() != ILibrary::ConnectionState::Connected) {
        return TrackPtr();
    }

    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);

    if (index >= this->playlist.Count()) {
        return TrackPtr();
    }

    return this->playlist.GetWithTimeout(index, TRACK_TIMEOUT_MS());
}

Editor PlaybackService::Edit() {
    return Editor(
        *this,
        this->playlist,
        this->messageQueue,
        this->playlistMutex);
}

ITrackListEditor* PlaybackService::EditPlaylist() {
    /* the internal implementation of this class has a stubbed
    Release() method to avoid programmer error. if the SDK is
    requesting an Editor we need to actually release resources. */
    class SdkTrackListEditor : public PlaybackService::Editor {
        public:
            SdkTrackListEditor(
                PlaybackService& playback,
                TrackList& tracks,
                Queue& queue,
                Mutex& mutex)
            : PlaybackService::Editor(playback, tracks, queue, mutex) {
            }

            virtual ~SdkTrackListEditor() {
            }

            virtual void Release() noexcept {
                delete this;
            }
    };

    return new SdkTrackListEditor(
        *this,
        this->playlist,
        this->messageQueue,
        this->playlistMutex);
}

void PlaybackService::OnStreamEvent(StreamState eventType, std::string uri) {
    POST_STREAM_MESSAGE(this, (int64_t) eventType, uri);
}

void PlaybackService::OnPlaybackEvent(PlaybackState eventType) {
    POST(this, MESSAGE_PLAYBACK_EVENT, (int64_t) eventType, 0);
}

void PlaybackService::OnVolumeChanged() {
    POST(this, MESSAGE_VOLUME_CHANGED, 0, 0);
}

void PlaybackService::OnTimeChanged(double time) {
    POST(this, MESSAGE_TIME_CHANGED, 0, 0);
}

void PlaybackService::OnIndexerFinished(int trackCount) {
    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);
    this->playlist.ClearCache();
    this->unshuffled.ClearCache();
}

/* our Editor interface. we proxy all of the ITrackListEditor methods so we
can track and maintain our currently playing index as tracks move around
in the backing store. lots of annoying book keeping. we have to do it this
way because it's possible to have the same item in the playlist multiple
times -- otherwise we could just cache the ID and then look it up when we
have finished all operations. */

PlaybackService::Editor::Editor(
    PlaybackService& playback,
    TrackList& tracks,
    Queue& queue,
    Mutex& mutex)
: playback(playback)
, queue(queue)
, lock(mutex)
, edited(false) {
    this->tracks = IEditor(new musik::core::TrackListEditor(tracks));
    this->playIndex = playback.GetIndex();
    this->nextTrackInvalidated = false;
}

PlaybackService::Editor::Editor(Editor&& other)
: playback(other.playback)
, tracks(other.tracks)
, queue(other.queue)
, playIndex(other.playIndex)
, edited(false) {
    std::swap(this->lock, other.lock);

    /* we may never actually edit the playing track, but instead, edit the
    track that's up next. if that happens, we need to reload the next track.
    we COULD do this unconditionally, but it requires I/O, so it's best to
    avoid if possible. */
    this->nextTrackInvalidated = other.nextTrackInvalidated;
}

PlaybackService::Editor::~Editor() {
    if (this->edited) {
        /* we've been tracking the playback index through edit operations. let's
        update it here. */

        /* make sure the play index we're requesting is in bounds */
        if (this->playIndex != this->playback.GetIndex() || this->nextTrackInvalidated) {
            if (this->playback.Count() > 0 && this->playIndex != NO_POSITION) {
                this->playIndex = std::min(this->playback.Count() - 1, this->playIndex);
            }

            this->queue.Post(Message::Create(
                &this->playback, MESSAGE_PREPARE_NEXT_TRACK, this->playIndex, 0));
        }

        this->playback.messageQueue.Post(Message::Create(
            &this->playback, MESSAGE_NOTIFY_EDITED, 0, 0));
    }

    /* implicitly unlocks the mutex when this block exists */
}

bool PlaybackService::Editor::Insert(int64_t id, size_t index) {
    if ((this->edited = this->tracks->Insert(id, index))) {
        if (index == this->playIndex) {
            ++this->playIndex;
        }

        if (index == this->playIndex + 1) {
            this->nextTrackInvalidated = true;
        }

        return true;
    }
    return false;
}

bool PlaybackService::Editor::Swap(size_t index1, size_t index2) {
    if ((this->edited = this->tracks->Swap(index1, index2))) {
        if (index1 == this->playIndex) {
            this->playIndex = index2;
            this->nextTrackInvalidated = true;
        }
        else if (index2 == this->playIndex) {
            this->playIndex = index1;
            this->nextTrackInvalidated = true;
        }

        return true;
    }
    return false;
}

bool PlaybackService::Editor::Move(size_t from, size_t to) {
    if ((this->edited = this->tracks->Move(from, to))) {
        if (from == this->playIndex) {
            this->playIndex = to;
        }
        else if (to == this->playIndex) {
            this->playIndex += (from > to) ? 1 : -1;
        }

        if (to == this->playIndex + 1) {
            this->nextTrackInvalidated = true;
        }

        return true;
    }
    return false;
}

bool PlaybackService::Editor::Delete(size_t index) {
    if ((this->edited = this->tracks->Delete(index))) {
        if (this->playback.Count() == 0) {
            this->playIndex = NO_POSITION;
        }
        if (index == this->playIndex) {
            this->playIndex = START_OVER;
        }
        else if (index == this->playIndex + 1) {
            this->nextTrackInvalidated = true;
        }
        else if (index < this->playIndex) {
            --this->playIndex;
        }
        return true;
    }
    return false;
}

void PlaybackService::Editor::Add(const int64_t id) {
    this->tracks->Add(id);

    if (this->playback.Count() - 1 == this->playIndex + 1) {
        this->nextTrackInvalidated = true;
    }

    this->edited = true;
}

void PlaybackService::Editor::Clear() {
    playback.playlist.Clear();
    playback.unshuffled.Clear();
    this->playIndex = -1;
    this->nextTrackInvalidated = true;
    this->edited = true;
}

void PlaybackService::Editor::Shuffle() {
    /* inefficient -- we can do it faster with a bit more logic. if
    this ever becomes a problem we can speed it up. */
    if (playback.IsShuffled()) {
        playback.ToggleShuffle(); /* off */
    }

    playback.ToggleShuffle(); /* on */
    this->playIndex = playback.GetIndex();
    this->nextTrackInvalidated = true;
    this->edited = true;
}

void PlaybackService::Editor::Release() noexcept {
    /* nothing! */
}

std::string PlaybackService::UriAtIndex(size_t index) {
    if (index < this->playlist.Count()) {
        auto track = TrackAtIndexWithTimeout(index);
        if (track) {
            return this->library->GetResourceLocator().GetTrackUri(track.get());
        }
    }
    return "";
}

ITrackList* PlaybackService::Clone() {
    std::unique_lock<std::recursive_mutex> lock(this->playlistMutex);
    std::shared_ptr<TrackList> to = std::make_shared<TrackList>(this->library);
    this->playlist.CopyTo(*to.get());
    return to->GetSdkValue();
}

ITransport::Gain PlaybackService::GainAtIndex(size_t index) {
    using Mode = ReplayGainMode;

    ITransport::Gain result;

    const float preampDb = static_cast<float>(
        playbackPrefs->GetDouble(keys::PreampDecibels.c_str(), 0.0f));

    result.preamp = powf(10.0f, (preampDb / 20.0f));
    result.peakValid = false;

    const Mode mode = static_cast<Mode>(playbackPrefs->GetInt(
        keys::ReplayGainMode.c_str(), (int) Mode::Disabled));

    if (mode != Mode::Disabled && index < this->playlist.Count()) {
        auto track = TrackAtIndexWithTimeout(index);
        if (track) {
            const auto rg = track->GetReplayGain();
            const float gain = (mode == Mode::Album) ? rg.albumGain : rg.trackGain;
            const float peak = (mode == Mode::Album) ? rg.albumPeak : rg.trackPeak;
            if (gain != 1.0f) {
                /* http://wiki.hydrogenaud.io/index.php?title=ReplayGain_2.0_specification#Reduced_gain */
                result.gain = powf(10.0f, (gain / 20.0f));
                result.peak = (1.0f / peak);
                result.peakValid = true;
            }
        }
    }

    return result;
}