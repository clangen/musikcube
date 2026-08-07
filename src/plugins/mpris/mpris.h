/// @file mpris.h
/// @brief MPRIS playback remote for Linux.
/// @details Implements the MPRIS (Media Player Remote Interfacing
/// Specification) over D-Bus using sd-bus, so desktop environments can control
/// musikcube play/pause/next/prev, volume, shuffle, loop and seek, and read the
/// current track metadata. Linux-only.

#pragma once

#include <musikcore/sdk/IPlaybackRemote.h>
#include <musikcore/sdk/IPlugin.h>
#include <mutex>
#include <thread>
#include <memory>

extern "C" {
    #include SDBUS_HEADER
}

using namespace musik::core::sdk;

/** @brief MPRIS properties that can change and are broadcast to the bus. */
enum MPRISProperty {
    /** @brief The volume changed. */
    Volume = 1,
    /** @brief The playback status changed. */
    PlaybackStatus = 2,
    /** @brief The loop status changed. */
    LoopStatus = 3,
    /** @brief The shuffle state changed. */
    Shuffle = 4,
    /** @brief The current track metadata changed. */
    Metadata = 5,
};

/** @brief Metadata snapshot published through the MPRIS Metadata property. */
struct MPRISMetadataValues {
    /** @brief Unique track id on the MPRIS bus. */
    std::string trackid;
    /** @brief Track length in microseconds. */
    uint64_t length;
    /** @brief Artist of the current track. */
    std::string artist;
    /** @brief Title of the current track. */
    std::string title;
    /** @brief Album of the current track. */
    std::string album;
    /** @brief Album artist of the current track. */
    std::string albumArtist;
    /** @brief URL or path of the album art. */
    std::string albumArt;
    /** @brief Genre of the current track. */
    std::string genre;
    /** @brief Comment of the current track. */
    std::string comment;
    /** @brief Track number on the album. */
    uint32_t trackNumber;
    /** @brief Disc number of the album. */
    uint32_t discNumber;
    /** @brief Whether any metadata is available. */
    bool available;
    /** @brief Constructs empty metadata values. */
    MPRISMetadataValues();
};

/** @brief MPRIS remote that bridges the playback service to the D-Bus session bus.
 *  @details Owns the sd-bus connection and a background loop that dispatches
 *  D-Bus messages. All exported methods (Play, Pause, Next, Previous, Seek,
 *  Stop, OpenUri, ...) route to the IPlaybackService. */
class MPRISRemote : public IPlaybackRemote {
    private:
        /** @brief The playback service this remote controls. */
        IPlaybackService* playback;
        /** @brief Connection to the D-Bus session bus. */
        sd_bus* bus;
        /** @brief Guards access to the sd-bus connection. */
        std::recursive_mutex sd_mutex;
        /** @brief Thread dispatching D-Bus events. */
        std::shared_ptr<std::thread> thread;
        /** @brief Whether MPRIS has been initialized on the bus. */
        bool mpris_initialized;
        /** @brief Set to stop the dispatch loop. */
        bool stop_processing;

        /** @brief Registers the MPRIS interfaces on the session bus.
         *  @return True if the bus name and interfaces were acquired. */
        bool MPRISInit();
        /** @brief Releases the bus connection and stops the loop. */
        void MPRISDeinit();
        /** @brief Emits a PropertiesChanged signal for a property.
         *  @param prop The property that changed. */
        void MPRISEmitChange(MPRISProperty prop);
        /** @brief Emits a Seeked signal.
         *  @param curpos New playback position in microseconds. */
        void MPRISEmitSeek(double curpos);
        /** @brief D-Bus dispatch loop body. */
        void MPRISLoop();

    public:
        /** @brief Constructs an uninitialized MPRIS remote. */
        MPRISRemote()
            : playback(NULL),
              bus(NULL),
              stop_processing(false),
              mpris_initialized(false) {}

        /** @brief Destroys the remote and deinitializes MPRIS. */
        ~MPRISRemote() {
            MPRISDeinit();
        }

        /** @brief Destroys the remote instance. */
        virtual void Release() { }
        /** @brief Attaches the playback service to control.
         *  @param playback The playback service. */
        virtual void SetPlaybackService(IPlaybackService* playback);
        /** @brief Called when the current track changes.
         *  @param track The new track. */
        virtual void OnTrackChanged(ITrack* track);
        /** @brief Called when the playback state changes.
         *  @param state The new playback state. */
        virtual void OnPlaybackStateChanged(PlaybackState state);
        /** @brief Called when the volume changes.
         *  @param volume The new volume, 0.0 to 1.0. */
        virtual void OnVolumeChanged(double volume);
        /** @brief Called when the playback position changes.
         *  @param time Position in seconds. */
        virtual void OnPlaybackTimeChanged(double time);
        /** @brief Called when repeat/shuffle mode changes.
         *  @param repeatMode The new repeat mode.
         *  @param shuffled Whether shuffle is enabled. */
        virtual void OnModeChanged(RepeatMode repeatMode, bool shuffled);
        /** @brief Called when the play queue changes (unused). */
        virtual void OnPlayQueueChanged() { }

        /** @brief Advances to the next track.
         *  @return True on success. */
        void MPRISNext();
        /** @brief Returns to the previous track.
         *  @return True on success. */
        void MPRISPrev();
        /** @brief Pauses playback.
         *  @return True on success. */
        void MPRISPause();
        /** @brief Toggles play/pause.
         *  @return True on success. */
        void MPRISPlayPause();
        /** @brief Stops playback.
         *  @return True on success. */
        void MPRISStop();
        /** @brief Starts or resumes playback.
         *  @return True on success. */
        void MPRISPlay();
        /** @brief Seeks the current track.
         *  @param position Seek position.
         *  @param relative True for a relative seek, false for absolute. */
        void MPRISSeek(int64_t position, bool relative=false);
        /** @brief Returns the current playback status string.
         *  @return "Playing", "Paused" or "Stopped". */
        const char* MPRISGetPlaybackStatus();
        /** @brief Returns the current loop status string.
         *  @return "None", "Track" or "Playlist". */
        const char* MPRISGetLoopStatus();
        /** @brief Sets the loop status.
         *  @param state The new loop status string. */
        void MPRISSetLoopStatus(const char* state);
        /** @brief Returns the current playback position in microseconds.
         *  @return Position in microseconds. */
        uint64_t MPRISGetPosition();
        /** @brief Returns whether shuffle is enabled.
         *  @return 1 if shuffled, 0 otherwise. */
        unsigned int MPRISGetShuffleStatus();
        /** @brief Sets the shuffle state.
         *  @param state Non-zero to enable shuffle. */
        void MPRISSetShuffleStatus(unsigned int state);
        /** @brief Returns the current volume.
         *  @return Volume, 0.0 to 1.0. */
        double MPRISGetVolume();
        /** @brief Sets the volume.
         *  @param vol Volume, 0.0 to 1.0. */
        void MPRISSetVolume(double vol);
        /** @brief Returns the current track metadata.
         *  @return The metadata values to publish. */
        struct MPRISMetadataValues MPRISGetMetadata();
};
