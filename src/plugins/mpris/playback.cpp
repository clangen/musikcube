#include "mpris.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>

namespace {
    int64_t microseconds(double seconds) noexcept {
        if (!std::isfinite(seconds) || seconds <= 0) {
            return 0;
        }
        const auto limit = static_cast<double>(std::numeric_limits<int64_t>::max());
        return seconds >= limit / 1000000.0 ? std::numeric_limits<int64_t>::max()
            : static_cast<int64_t>(seconds * 1000000.0);
    }
}

struct MPRISRemote::PendingAction {
    MPRISRemote* remote;
    Action action;
    double value;
    uint64_t generation;
};

void MPRISRemote::CommandDone(void*, mc_result) noexcept {}

mc_result MPRISRemote::Send(mc_playback_command command, double value) {
    if (this->stopping.load()) {
        return MC_ERROR_CANCELLED;
    }
    return this->remote->control_async(this->remote->context, command, value, CommandDone, nullptr);
}

mc_result MPRISRemote::ScheduleAction(Action action, double value, uint64_t serial) {
    if (this->stopping.load()) {
        return MC_ERROR_CANCELLED;
    }
    auto* pending = new (std::nothrow) PendingAction { this, action, value, serial };
    if (!pending) {
        return MC_ERROR_INTERNAL;
    }
    const auto result = this->remote->with_playback_async(this->remote->context, ActionReady, pending);
    if (result != MC_OK) {
        delete pending;
    }
    return result;
}

void MPRISRemote::ActionReady(void* user, mc_result status, mc_playback* playback) noexcept {
    std::unique_ptr<PendingAction> pending(static_cast<PendingAction*>(user));
    musik::core::api::Playback owned(playback);
    if (status != MC_OK || !playback) {
        return;
    }
    try {
        const auto initial = owned.Snapshot();
        if (!initial) {
            return;
        }
        auto snapshot = *initial;
        switch (pending->action) {
            case Action::SeekRelative:
                owned.Control(MC_PLAYBACK_SET_POSITION, std::max(0.0, snapshot.position + pending->value));
                break;
            case Action::SeekAbsolute:
                if (pending->generation == pending->remote->generation.load()) {
                    owned.Control(MC_PLAYBACK_SET_POSITION, std::max(0.0, pending->value));
                }
                break;
            case Action::Repeat:
                for (unsigned i = 0; i < 2 && snapshot.repeat != static_cast<mc_playback_repeat_mode>(pending->value); ++i) {
                    if (owned.Control(MC_PLAYBACK_TOGGLE_REPEAT) != MC_OK) {
                        break;
                    }
                    const auto updated = owned.Snapshot();
                    if (!updated) {
                        break;
                    }
                    snapshot = *updated;
                }
                break;
            case Action::Shuffle:
                if (snapshot.shuffled != (pending->value != 0)) {
                    owned.Control(MC_PLAYBACK_TOGGLE_SHUFFLE);
                }
                break;
        }
    }
    catch (...) {
        pending->remote->Log("could not read playback state for D-Bus command");
    }
}

mc_result MPRISRemote::MPRISNext() {
    return this->Send(MC_PLAYBACK_NEXT);
}

mc_result MPRISRemote::MPRISPrev() {
    return this->Send(MC_PLAYBACK_PREVIOUS);
}

mc_result MPRISRemote::MPRISPause() {
    return this->Send(MC_PLAYBACK_PAUSE);
}

mc_result MPRISRemote::MPRISPlayPause() {
    return this->Send(MC_PLAYBACK_TOGGLE_PAUSE);
}

mc_result MPRISRemote::MPRISStop() {
    return this->Send(MC_PLAYBACK_STOP);
}

mc_result MPRISRemote::MPRISPlay() {
    return this->Send(MC_PLAYBACK_RESUME);
}

mc_result MPRISRemote::MPRISSeek(int64_t position, bool relative, const char* track) {
    uint64_t serial = 0;
    if (!relative) {
        std::lock_guard lock(this->stateMutex);
        if (!track || !this->metadata.available || this->metadata.trackid != track) {
            return MC_OK;
        }
        serial = this->generation.load();
    }
    return this->ScheduleAction(relative ? Action::SeekRelative : Action::SeekAbsolute,
        static_cast<double>(position) / 1000000.0, serial);
}

const char* MPRISRemote::MPRISGetPlaybackStatus() {
    std::lock_guard lock(this->stateMutex);
    return this->snapshot.state == MC_PLAYBACK_PLAYING ? "Playing"
        : this->snapshot.state == MC_PLAYBACK_PAUSED ? "Paused" : "Stopped";
}

const char* MPRISRemote::MPRISGetLoopStatus() {
    std::lock_guard lock(this->stateMutex);
    return this->snapshot.repeat == MC_PLAYBACK_REPEAT_TRACK ? "Track"
        : this->snapshot.repeat == MC_PLAYBACK_REPEAT_LIST ? "Playlist" : "None";
}

mc_result MPRISRemote::MPRISSetLoopStatus(const char* value) {
    if (!value) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    const auto mode = std::strcmp(value, "None") == 0 ? MC_PLAYBACK_REPEAT_OFF
        : std::strcmp(value, "Track") == 0 ? MC_PLAYBACK_REPEAT_TRACK
        : std::strcmp(value, "Playlist") == 0 ? MC_PLAYBACK_REPEAT_LIST : UINT32_MAX;
    return mode == UINT32_MAX ? MC_ERROR_INVALID_ARGUMENT : this->ScheduleAction(Action::Repeat, mode);
}

int64_t MPRISRemote::MPRISGetPosition() {
    std::lock_guard lock(this->stateMutex);
    double position = this->snapshot.position;
    if (this->snapshot.state == MC_PLAYBACK_PLAYING) {
        position += std::chrono::duration<double>(std::chrono::steady_clock::now() - this->sampled).count();
    }
    if (this->snapshot.duration > 0) {
        position = std::min(position, this->snapshot.duration);
    }
    return microseconds(position);
}

unsigned int MPRISRemote::MPRISGetShuffleStatus() {
    std::lock_guard lock(this->stateMutex);
    return this->snapshot.shuffled ? 1 : 0;
}

mc_result MPRISRemote::MPRISSetShuffleStatus(unsigned int value) {
    return this->ScheduleAction(Action::Shuffle, value ? 1 : 0);
}

double MPRISRemote::MPRISGetVolume() {
    std::lock_guard lock(this->stateMutex);
    return this->snapshot.volume;
}

mc_result MPRISRemote::MPRISSetVolume(double value) {
    return this->Send(MC_PLAYBACK_SET_VOLUME, value);
}

MPRISMetadataValues MPRISRemote::MPRISGetMetadata() {
    std::lock_guard lock(this->stateMutex);
    auto result = this->metadata;
    result.length = microseconds(this->snapshot.duration);
    return result;
}
