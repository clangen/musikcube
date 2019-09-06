
#pragma once

#include <core/sdk/IPlaybackRemote.h>
#include <core/sdk/IPlugin.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>

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

  MPRISMetadataValues() {
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

        virtual void Release() {
        }

        virtual void SetPlaybackService(IPlaybackService* playback) {
          std::unique_lock<decltype(sd_mutex)> lock(sd_mutex);
          this->playback = playback;
          mpris_initialized = MPRISInit();
        }

        virtual void OnTrackChanged(ITrack* track);
        virtual void OnPlaybackStateChanged(PlaybackState state);
        virtual void OnVolumeChanged(double volume);
        virtual void OnPlaybackTimeChanged(double time);
        virtual void OnModeChanged(RepeatMode repeatMode, bool shuffled);
        virtual void OnPlayQueueChanged() { }

        void MPRISNext() {
          if (playback) {
            playback->Next();
          }
        }

        void MPRISPrev() {
          if (playback) {
            playback->Previous();
          }
        }

        void MPRISPause() {
          if (playback) {
            auto state = playback->GetPlaybackState();
            if (state == PlaybackState::PlaybackPlaying) {
              playback->PauseOrResume();
            }
          }
        }

        void MPRISPlayPause() {
          if (playback) {
            playback->PauseOrResume();
          }
        }

        void MPRISStop() {
          if (playback) {
            playback->Stop();
          }
        }

        void MPRISPlay() {
          if (playback) {
            auto state = playback->GetPlaybackState();
            if (state != PlaybackState::PlaybackPlaying) {
              playback->PauseOrResume();
            }
          }
        }

        void MPRISSeek(uint64_t position, bool relative=false) {
          double _pos = ((double)position)/(1000*1000);
          if (playback) {
          }
        }

       const char* MPRISGetPlaybackStatus() {
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

      const char* MPRISGetLoopStatus() {
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

      void MPRISSetLoopStatus(const char* state) {
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

     uint64_t MPRISGetPosition() {
       if (playback) {
         return (uint64_t)(playback->GetPosition()*1000*1000);
       }
       return 0;
     }

     unsigned int MPRISGetShuffleStatus() {
       if (playback) {
         return playback->IsShuffled() ? 1: 0;
       }
       return 0;
     }

     void MPRISSetShuffleStatus(unsigned int state) {
       if (playback)
         {
           unsigned int isShuffled = playback->IsShuffled() ? 1: 0;
           if ((state & 0x1) ^ isShuffled) {
             playback->ToggleShuffle();
           }
         }
     }

     double MPRISGetVolume() {
       if (playback) {
         return playback->GetVolume();
       }
       return 0.0;
     }

     void MPRISSetVolume(double vol) {
       if (playback) {
         playback->SetVolume(vol);
       }
     }

     struct MPRISMetadataValues MPRISGetMetadata();

};
