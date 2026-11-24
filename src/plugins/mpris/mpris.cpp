#include "mpris.h"
#include "dbus.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>
#include <poll.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>

namespace {
    mc_string_view view(const char* text) {
        return { text, std::strlen(text) };
    }

    int timeoutMillis(sd_bus* bus) noexcept {
        uint64_t deadline = UINT64_MAX;
        if (sd_bus_get_timeout(bus, &deadline) < 0 || deadline == UINT64_MAX) {
            return -1;
        }
        timespec now {};
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            return 0;
        }
        const uint64_t current = static_cast<uint64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
        if (deadline <= current) {
            return 0;
        }
        const auto remaining = deadline - current;
        const auto millis = remaining / 1000 + (remaining % 1000 != 0);
        return static_cast<int>(std::min<uint64_t>(millis, std::numeric_limits<int>::max()));
    }
}

MPRISRemote::MPRISRemote(const mc_plugin_services& services)
    : host(services.host), remote(services.playback_remote), library(services.library) {}

void MPRISRemote::Log(const char* message) const noexcept {
    if (this->host && this->host->log) {
        this->host->log(this->host->host_context, MC_LOG_WARNING, view("MPRIS"), view(message));
    }
}

void MPRISRemote::Wake() noexcept {
    if (this->wake < 0) {
        return;
    }
    const uint64_t signal = 1;
    ssize_t result;
    do {
        result = write(this->wake, &signal, sizeof(signal));
    } while (result < 0 && errno == EINTR);
    if (result < 0 && errno != EAGAIN) {
        this->Log("could not wake the D-Bus worker");
        std::abort();
    }
}

void MPRISRemote::InitialState(void* context, const mc_playback_snapshot* snapshot, const mc_track_ref* track) noexcept {
    mc_plugin_playback_event event = MC_PLUGIN_PLAYBACK_EVENT_DEFAULT;
    event.playback = *snapshot;
    event.track = track;
    static_cast<MPRISRemote*>(context)->Changed(event);
}

void MPRISRemote::Changed(const mc_plugin_playback_event& event) noexcept {
    uint64_t serial = 0;
    int64_t trackId = 0;
    uint32_t flags = 0;
    try {
        {
            std::lock_guard lock(this->stateMutex);
            const bool durationChanged = this->snapshot.duration != event.playback.duration;
            this->snapshot = event.playback;
            this->sampled = std::chrono::steady_clock::now();

            switch (event.event) {
                case MC_PLAYBACK_CHANGED_TRACK:
                    serial = ++this->generation;
                    trackId = event.track ? mc_track_ref_get_id(event.track) : 0;
                    this->currentTrackId = trackId;
                    this->metadata = {};
                    this->thumbnailId = 0;
                    this->artworkLibrary.reset();
                    this->artworkPending = true;
                    if (trackId > 0) {
                        this->metadata.trackid = "/org/mpris/MediaPlayer2/track/t" + std::to_string(serial);
                        this->metadata.available = true;
                    }
                    flags = MetadataChanged | PositionChanged | ArtworkChanged;
                    break;
                case MC_PLAYBACK_CHANGED_STATE:
                    flags = StateChanged;
                    break;
                case MC_PLAYBACK_CHANGED_POSITION:
                    flags = PositionChanged;
                    break;
                case MC_PLAYBACK_CHANGED_VOLUME:
                    flags = VolumeChanged;
                    break;
                case MC_PLAYBACK_CHANGED_MODE:
                    flags = ModeChanged;
                    break;
                default:
                    break;
            }

            if (durationChanged) {
                flags |= MetadataChanged;
            }
        }

        /* the owner-thread handle reads already loaded metadata without I/O. */
        if (trackId > 0 && this->ownerPlayback) {
            this->LoadMetadata(this->ownerPlayback->Get(), serial, trackId);
        }
        this->notifications.fetch_or(flags);
        if (flags & ArtworkChanged) {
            this->artworkChanged.notify_one();
        }
        this->Wake();
    }
    catch (...) {
        this->Log("could not copy playback notification");
    }
}

void MPRISRemote::QueueChanged() noexcept {}

mc_result MPRISRemote::Start() {
    if (!this->remote || !this->remote->read_state || !this->remote->control_async ||
        !this->remote->with_playback_async)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    this->wake = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (this->wake < 0) {
        return MC_ERROR_IO;
    }
    if (sd_bus_open_user(&this->bus) < 0) {
        return MC_ERROR_IO;
    }

    if (sd_bus_add_object_vtable(this->bus, nullptr, "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2", musikcube_mp_table, this) < 0 ||
        sd_bus_add_object_vtable(this->bus, nullptr, "/org/mpris/MediaPlayer2",
            "org.mpris.MediaPlayer2.Player", musikcube_mpp_table, this) < 0)
    {
        return MC_ERROR_IO;
    }

    const auto name = std::string("org.mpris.MediaPlayer2.musikcube.instance") + std::to_string(getpid());
    if (sd_bus_request_name(this->bus, name.c_str(), 0) < 0) {
        return MC_ERROR_IO;
    }

    /* start with a snapshot; the asynchronous handle supplies track metadata. */
    this->stopping.store(false);
    const auto status = this->remote->read_state(this->remote->context, InitialState, this);
    if (status != MC_OK) {
        return status;
    }
    const auto access = this->remote->with_playback_async(this->remote->context, PlaybackReady, this);
    if (access != MC_OK) {
        return access;
    }

    this->worker = std::thread(&MPRISRemote::Run, this);
    this->artworkWorker = std::thread(&MPRISRemote::RunArtwork, this);
    return MC_OK;
}

void MPRISRemote::RequestStop() noexcept {
    {
        std::lock_guard lock(this->stateMutex);
        this->stopping.store(true);
    }
    this->artworkChanged.notify_all();
    this->ownerPlayback.reset();
    this->Wake();
}

mc_result MPRISRemote::Join() {
    if (this->artworkWorker.joinable()) {
        this->artworkWorker.join();
    }
    if (this->worker.joinable()) {
        this->worker.join();
    }

    /* neither worker can use the bus, wake descriptor, or artwork now. */
    if (this->bus) {
        sd_bus_close(this->bus);
        sd_bus_unref(this->bus);
        this->bus = nullptr;
    }
    if (this->wake >= 0) {
        close(this->wake);
        this->wake = -1;
    }
    if (!this->artworkFile.empty()) {
        unlink(this->artworkFile.c_str());
        this->artworkFile.clear();
    }
    return MC_OK;
}

void MPRISRemote::EmitChanges(uint32_t flags) noexcept {
    const char* path = "/org/mpris/MediaPlayer2";
    const char* player = "org.mpris.MediaPlayer2.Player";
    if (flags & StateChanged) {
        sd_bus_emit_properties_changed(this->bus, path, player, "PlaybackStatus", nullptr);
    }
    if (flags & ModeChanged) {
        sd_bus_emit_properties_changed(this->bus, path, player, "LoopStatus", "Shuffle", nullptr);
    }
    if (flags & VolumeChanged) {
        sd_bus_emit_properties_changed(this->bus, path, player, "Volume", nullptr);
    }
    if (flags & MetadataChanged) {
        sd_bus_emit_properties_changed(this->bus, path, player, "Metadata", nullptr);
    }
    if (flags & PositionChanged) {
        const auto position = this->MPRISGetPosition();
        sd_bus_emit_signal(this->bus, path, player, "Seeked", "x", position);
    }
}

void MPRISRemote::Run() noexcept {
    while (!this->stopping.load()) {
        const uint32_t flags = this->notifications.exchange(0);
        this->EmitChanges(flags);

        int processed = 0;
        do {
            processed = sd_bus_process(this->bus, nullptr);
        } while (processed > 0 && !this->stopping.load());
        if (processed < 0) {
            this->Log("D-Bus connection failed");
            return;
        }

        /* wait for either D-Bus work or a playback change without polling. */
        const int busFd = sd_bus_get_fd(this->bus);
        const int busEvents = sd_bus_get_events(this->bus);
        if (busFd < 0 || busEvents < 0) {
            this->Log("could not poll D-Bus connection");
            return;
        }
        pollfd fds[2] {
            { .fd = busFd, .events = static_cast<short>(busEvents), .revents = 0 },
            { .fd = this->wake, .events = POLLIN, .revents = 0 },
        };
        const int result = poll(fds, 2, timeoutMillis(this->bus));
        if (result < 0 && errno != EINTR) {
            this->Log("D-Bus polling failed");
            return;
        }
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            this->Log("D-Bus connection closed");
            return;
        }
        if (fds[1].revents & POLLIN) {
            uint64_t signals;
            (void) read(this->wake, &signals, sizeof(signals));
        }
    }
}

void MPRISRemote::RunArtwork() noexcept {
    for (;;) {
        {
            std::unique_lock lock(this->stateMutex);
            this->artworkChanged.wait(lock, [this] {
                return this->stopping.load() || this->artworkPending;
            });
            if (this->stopping.load()) {
                return;
            }
        }
        this->RefreshArtwork();
    }
}
