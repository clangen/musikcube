#pragma once

#include <musikcore/plugin/library.h>
#include <musikcore/plugin/playback-remote.hpp>
#include <musikcore/playback.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

extern "C" {
#include SDBUS_HEADER
}

/* a copied track snapshot that the D-Bus worker can read without playback access. */
struct MPRISMetadataValues {
    std::string trackid;
    int64_t length = 0;
    std::string artist;
    std::string title;
    std::string album;
    std::string albumArtist;
    std::string albumArt;
    std::string genre;
    std::string comment;
    int32_t trackNumber = 0;
    int32_t discNumber = 0;
    bool available = false;
};

/* serves D-Bus requests while a separate worker reads artwork. */
class MPRISRemote {
public:
    explicit MPRISRemote(const mc_plugin_services&);

    MPRISRemote(const MPRISRemote&) = delete;
    MPRISRemote(MPRISRemote&&) = delete;
    ~MPRISRemote() = default;

    MPRISRemote& operator=(const MPRISRemote&) = delete;
    MPRISRemote& operator=(MPRISRemote&&) = delete;

    /* playback notifications and lifecycle calls come from the host. */
    void Changed(const mc_plugin_playback_event&) noexcept;
    void QueueChanged() noexcept;

    mc_result Start();
    void RequestStop() noexcept;
    mc_result Join();

    /* the D-Bus worker uses these to read state and submit playback commands. */
    mc_result MPRISNext();
    mc_result MPRISPrev();
    mc_result MPRISPause();
    mc_result MPRISPlayPause();
    mc_result MPRISStop();
    mc_result MPRISPlay();
    mc_result MPRISSeek(int64_t position, bool relative = false, const char* track = nullptr);

    const char* MPRISGetPlaybackStatus();
    const char* MPRISGetLoopStatus();
    mc_result MPRISSetLoopStatus(const char*);
    int64_t MPRISGetPosition();
    unsigned int MPRISGetShuffleStatus();
    mc_result MPRISSetShuffleStatus(unsigned int);
    double MPRISGetVolume();
    mc_result MPRISSetVolume(double);
    MPRISMetadataValues MPRISGetMetadata();

private:
    static constexpr uint32_t StateChanged = 1u << 0;
    static constexpr uint32_t ModeChanged = 1u << 1;
    static constexpr uint32_t VolumeChanged = 1u << 2;
    static constexpr uint32_t MetadataChanged = 1u << 3;
    static constexpr uint32_t PositionChanged = 1u << 4;
    static constexpr uint32_t ArtworkChanged = 1u << 5;

    enum class Action {
        SeekRelative,
        SeekAbsolute,
        Repeat,
        Shuffle,
    };

    struct PendingAction;

    /* the host delivers these callbacks on the playback owner thread. */
    static void MC_CALL InitialState(void*, const mc_playback_snapshot*, const mc_track_ref*) noexcept;
    static void MC_CALL CommandDone(void*, mc_result) noexcept;
    static void MC_CALL ActionReady(void*, mc_result, mc_playback*) noexcept;
    static void MC_CALL PlaybackReady(void*, mc_result, mc_playback*) noexcept;

    void LoadMetadata(mc_playback*, uint64_t, int64_t) noexcept;
    mc_result ScheduleAction(Action, double, uint64_t = 0);
    mc_result Send(mc_playback_command, double = 0);

    void Wake() noexcept;
    void Run() noexcept;
    void RunArtwork() noexcept;
    void RefreshArtwork() noexcept;
    void EmitChanges(uint32_t) noexcept;

    void Log(const char*) const noexcept;

    /* host-owned services remain valid until this remote is destroyed. */
    const mc_host_api* host = nullptr;
    const mc_playback_remote_host_api* remote = nullptr;
    const mc_library_host_api* library = nullptr;

    /* owned playback handle; used and released on the owner thread. */
    std::optional<musik::core::api::Playback> ownerPlayback;

    /* `wake` interrupts the D-Bus wait; artwork reads must not delay D-Bus. */
    sd_bus* bus = nullptr;
    int wake = -1;
    std::thread worker;
    std::thread artworkWorker;

    /* `RequestStop()` wakes the worker; notifications hold pending D-Bus changes. */
    std::atomic<bool> stopping { false };
    std::atomic<uint32_t> notifications { 0 };

    /* discard commands and artwork that belong to an earlier track. */
    std::atomic<uint64_t> generation { 0 };

    /* shared snapshots and artwork identity are protected by this mutex. */
    std::mutex stateMutex;
    std::condition_variable artworkChanged;
    mc_playback_snapshot snapshot = MC_PLAYBACK_SNAPSHOT_DEFAULT;
    std::chrono::steady_clock::time_point sampled = std::chrono::steady_clock::now();
    MPRISMetadataValues metadata;
    int64_t currentTrackId = 0;
    int64_t thumbnailId = 0;
    bool artworkPending = false;

    /* `RefreshArtwork()` replaces this temporary file; `Join()` removes it. */
    std::string artworkFile;

    /* keep the source library alive while the worker reads its thumbnail. */
    std::shared_ptr<musik::core::api::Library> artworkLibrary;
};
