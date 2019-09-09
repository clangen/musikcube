
#pragma once

#include <core/sdk/IPlaybackRemote.h>
#include <core/sdk/IPlugin.h>
#include <mutex>
#include <thread>

extern "C" {
    #include <systemd/sd-bus.h>
}

using namespace musik::core::sdk;

enum MPRISProperty {
    Volume = 1,
    PlaybackStatus = 2,
    LoopStatus = 3,
    Shuffle = 4,
    Metadata = 5,
};

struct MPRISMetadataValues {
    std::string trackid;
    uint64_t length;
    std::string artist;
    std::string title;
    std::string album;
    std::string albumArtist;
    std::string genre;
    std::string comment;
    uint32_t trackNumber;
    uint32_t discNumber;
    bool available;
    MPRISMetadataValues();
};

class MPRISRemote : public IPlaybackRemote {
    private:
        IPlaybackService* playback;
        sd_bus* bus;
        std::mutex sd_mutex;
        std::shared_ptr<std::thread> thread;
        bool mpris_initialized;
        bool stop_processing;

        bool MPRISInit();
        void MPRISDeinit();
        void MPRISEmitChange(MPRISProperty prop);
        void MPRISEmitSeek(double curpos);
        void MPRISLoop();

    public:
        MPRISRemote()
            : playback(NULL),
              bus(NULL),
              stop_processing(false),
              mpris_initialized(false) {}

        ~MPRISRemote() {
            MPRISDeinit();
        }

        virtual void Release() { }
        virtual void SetPlaybackService(IPlaybackService* playback);
        virtual void OnTrackChanged(ITrack* track);
        virtual void OnPlaybackStateChanged(PlaybackState state);
        virtual void OnVolumeChanged(double volume);
        virtual void OnPlaybackTimeChanged(double time);
        virtual void OnModeChanged(RepeatMode repeatMode, bool shuffled);
        virtual void OnPlayQueueChanged() { }

        void MPRISNext();
        void MPRISPrev();
        void MPRISPause();
        void MPRISPlayPause();
        void MPRISStop();
        void MPRISPlay();
        void MPRISSeek(int64_t position, bool relative=false);
        const char* MPRISGetPlaybackStatus();
        const char* MPRISGetLoopStatus();
        void MPRISSetLoopStatus(const char* state);
        uint64_t MPRISGetPosition();
        unsigned int MPRISGetShuffleStatus();
        void MPRISSetShuffleStatus(unsigned int state);
        double MPRISGetVolume();
        void MPRISSetVolume(double vol);
        struct MPRISMetadataValues MPRISGetMetadata();
};
