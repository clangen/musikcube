
#include "mpris.h"
#include "dbus.h"
#include <map>
#include <vector>
#include <functional>

extern "C" {
    #include <unistd.h>
}

thread_local char localBuffer[1024];
static MPRISRemote remote;

static const std::map<MPRISProperty, const std::vector<const char*>> MPRISPropertyNames =
    {{MPRISProperty::Volume, {"Volume", NULL}},
     {MPRISProperty::PlaybackStatus, {"PlaybackStatus", NULL}},
     {MPRISProperty::LoopStatus, {"LoopStatus", NULL}},
     {MPRISProperty::Shuffle, {"Shuffle", NULL}},
     {MPRISProperty::Metadata, {"Metadata", NULL}}};

static std::string GetMetadataString(ITrack* track, const char* key)
{
    track->GetString(key, localBuffer, sizeof(localBuffer));
    return std::string(localBuffer);
}

static class MPRISPlugin : public IPlugin {
    public:
        MPRISPlugin() { }
        void Release() { }
        const char* Name() { return "MPRIS interface"; }
        const char* Version() { return "0.1.0"; }
        const char* Author() { return "brunosmmm"; }
        const char* Guid() { return "457df67f-f489-415f-975e-282f470b1c10"; }
        bool Configurable() { return false; }
        void Configure() { }
        void Reload() { }
        int SdkVersion() { return musik::core::sdk::SdkVersion; }
} plugin;

extern "C" IPlugin* GetPlugin() {
    return &plugin;
}

extern "C" IPlaybackRemote* GetPlaybackRemote() {
    return &remote;
}

bool MPRISRemote::MPRISInit() {
    int ret = 0;
    std::string requested_name;

    if (this->mpris_initialized) {
        return true;
    }

    ret = sd_bus_default_user(&(this->bus));
    if (ret < 0) {
        MPRISDeinit();
        return false;
    }

    // add DBUS entries
    ret = sd_bus_add_object_vtable(this->bus, NULL, "/org/mpris/MediaPlayer2",
                                   "org.mpris.MediaPlayer2", musikcube_mp_table, this);
    if (ret < 0) {
        MPRISDeinit();
        return false;
    }
    ret = sd_bus_add_object_vtable(this->bus, NULL, "/org/mpris/MediaPlayer2",
                                   "org.mpris.MediaPlayer2.Player", musikcube_mpp_table, this);
    if (ret < 0) {
        MPRISDeinit();
        return false;
    }

    requested_name = std::string("org.mpris.MediaPlayer2.musikcube.instance") + std::to_string(getpid());
    ret = sd_bus_request_name(this->bus, requested_name.c_str(), 0);
    if (ret < 0) {
        MPRISDeinit();
        return false;
    }

    // start loop
    thread.reset(new std::thread(std::bind(&MPRISRemote::MPRISLoop, this)));

    return true;
}

void MPRISRemote::MPRISDeinit() {
    sd_bus_close(this->bus);
    sd_bus_unref(this->bus);
    bus = NULL;
    stop_processing = true;
    if (thread) {
        thread->join();
        thread.reset();
    }
}

void MPRISRemote::MPRISEmitChange(MPRISProperty prop) {
    if (bus) {
        char** strv = (char**)(&MPRISPropertyNames.at(prop));
        std::unique_lock<decltype(sd_mutex)> lock(sd_mutex);
        sd_bus_emit_properties_changed_strv(bus, "/org/mpris/MediaPlayer2",
                                            "org.mpris.MediaPlayer2.Player",
                                            strv);
        sd_bus_flush(bus);
    }
};

void MPRISRemote::MPRISEmitSeek(double curpos) {
    if (bus) {
        int64_t position = (int64_t)(curpos*1000*1000);
        std::unique_lock<decltype(sd_mutex)> lock(sd_mutex);
        sd_bus_emit_signal(bus, "/org/mpris/MediaPlayer2",
                           "org.mpris.MediaPlayer2.Player",
                           "Seeked", "x", position);
    }
};

void MPRISRemote::MPRISLoop() {
    while (!stop_processing) {
        if (bus) {
            if (sd_bus_process(bus, NULL) > 0) {
                continue;
            }
            if (sd_bus_wait(bus, 500*1000) < 0) {
                break;
            }
        }
    }
}

void MPRISRemote::OnTrackChanged(ITrack* track) {
    if (playback) {
        MPRISEmitChange(MPRISProperty::Metadata);
        MPRISEmitSeek(playback->GetPosition());
    }
}

void MPRISRemote::OnPlaybackStateChanged(PlaybackState state) {
    if (playback) {
        MPRISEmitChange(MPRISProperty::PlaybackStatus);
    }
}

void MPRISRemote::OnVolumeChanged(double volume) {
    if (playback) {
        MPRISEmitChange(MPRISProperty::Volume);
    }
}

void MPRISRemote::OnPlaybackTimeChanged(double time) {
    if (playback) {
        MPRISEmitChange(MPRISProperty::Metadata);
        MPRISEmitSeek(time);
    }
}

void MPRISRemote::OnModeChanged(RepeatMode repeatMode, bool shuffled) {
    if (playback) {
        MPRISEmitChange(MPRISProperty::LoopStatus);
        MPRISEmitChange(MPRISProperty::Shuffle);
    }
}

struct MPRISMetadataValues MPRISRemote::MPRISGetMetadata() {
    struct MPRISMetadataValues metadata;
    if (playback) {
        auto curTrack = playback->GetPlayingTrack();
        if (curTrack) {
            metadata.artist = GetMetadataString(curTrack, track::Artist);
            metadata.title = GetMetadataString(curTrack, track::Title);
            metadata.albumArtist = GetMetadataString(curTrack, track::AlbumArtist);
            metadata.genre = GetMetadataString(curTrack, track::Genre);
            // TODO implement track ID properly using track index in playlist if possible
            metadata.trackid = std::string("/1");
            metadata.album = GetMetadataString(curTrack, track::Album);
            metadata.discNumber = curTrack->GetInt32(track::DiscNum);
            metadata.trackNumber = curTrack->GetInt32(track::TrackNum);
            metadata.length = curTrack->GetInt64(track::Duration)*1000*1000;
            metadata.available = true;
        }
    }
    return metadata;
}

void MPRISRemote::SetPlaybackService(IPlaybackService* playback) {
    std::unique_lock<decltype(sd_mutex)> lock(sd_mutex);
    this->playback = playback;
    mpris_initialized = MPRISInit();
}

void MPRISRemote::MPRISNext() {
    if (playback) {
        playback->Next();
    }
}

void MPRISRemote::MPRISPrev() {
    if (playback) {
        playback->Previous();
    }
}

void MPRISRemote::MPRISPause() {
    if (playback) {
        auto state = playback->GetPlaybackState();
        if (state == PlaybackState::PlaybackPlaying) {
            playback->PauseOrResume();
        }
    }
}

void MPRISRemote::MPRISPlayPause() {
    if (playback) {
        playback->PauseOrResume();
    }
}

void MPRISRemote::MPRISStop() {
    if (playback) {
        playback->Stop();
    }
}

void MPRISRemote::MPRISPlay() {
    if (playback) {
        auto state = playback->GetPlaybackState();
        if (state != PlaybackState::PlaybackPlaying) {
            playback->PauseOrResume();
        }
    }
}

void MPRISRemote::MPRISSeek(int64_t position, bool relative) {
    double _pos = ((double)position)/(1000*1000);
    if (playback) {
      if (!relative) {
        playback->SetPosition(_pos);
      }
      else {
        double curPos = playback->GetPosition();
        playback->SetPosition(curPos+_pos);
      }
    }
}

const char* MPRISRemote::MPRISGetPlaybackStatus() {
    if (playback) {
        auto state = playback->GetPlaybackState();
        switch (state) {
        case PlaybackState::PlaybackPlaying:
            return "Playing";
        case PlaybackState::PlaybackPaused:
            return "Paused";
        case PlaybackState::PlaybackPrepared:
        case PlaybackState::PlaybackStopped:
        default:
            break;
        }
    }
    return "Stopped";
}

const char* MPRISRemote::MPRISGetLoopStatus() {
    if (playback) {
        auto state = playback->GetRepeatMode();
        switch (state) {
        case RepeatMode::RepeatTrack:
            return "Track";
        case RepeatMode::RepeatList:
            return "Playlist";
        case RepeatMode::RepeatNone:
        default:
            break;
        }
    }
    return "None";
}

void MPRISRemote::MPRISSetLoopStatus(const char* state) {
    if (playback) {
        if (!strcmp(state, "None")) {
            playback->SetRepeatMode(RepeatMode::RepeatNone);
        }
        else if (!strcmp(state, "Playlist")) {
            playback->SetRepeatMode(RepeatMode::RepeatList);
        }
        else if (!strcmp(state, "Track")) {
            playback->SetRepeatMode(RepeatMode::RepeatTrack);
        }
    }
}

uint64_t MPRISRemote::MPRISGetPosition() {
    if (playback) {
        return (uint64_t)(playback->GetPosition()*1000*1000);
    }
    return 0;
}

unsigned int MPRISRemote::MPRISGetShuffleStatus() {
    if (playback) {
        return playback->IsShuffled() ? 1: 0;
    }
    return 0;
}

void MPRISRemote::MPRISSetShuffleStatus(unsigned int state) {
    if (playback)
        {
            unsigned int isShuffled = playback->IsShuffled() ? 1: 0;
            if ((state & 0x1) ^ isShuffled) {
                playback->ToggleShuffle();
            }
        }
}

double MPRISRemote::MPRISGetVolume() {
    if (playback) {
        return playback->GetVolume();
    }
    return 0.0;
}

void MPRISRemote::MPRISSetVolume(double vol) {
    if (playback) {
        playback->SetVolume(vol);
    }
}

MPRISMetadataValues::MPRISMetadataValues() {
    trackid = "";
    length = 0;
    artist = "";
    title = "";
    album = "";
    albumArtist = "";
    genre = "";
    comment = "";
    trackNumber = 0;
    discNumber = 0;
    available = false;
}
