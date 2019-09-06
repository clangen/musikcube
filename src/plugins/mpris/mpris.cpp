
#include <core/sdk/IPlaybackRemote.h>
#include <core/sdk/IPlugin.h>
#include <mutex>
#include <map>
#include <thread>
#include <chrono>
#include <functional>

extern "C" {
  #include <systemd/sd-bus.h>
}

thread_local char localBuffer[4096];

using namespace musik::core::sdk;

enum MPRISProperty {
    Volume = 1,
    PlaybackStatus = 2,
    LoopStatus = 3,
    Shuffle = 4,
    Metadata = 5,
};

// // hacky
struct sd_null_term_str {
  const char* string;
  const char* terminator;
};

static const std::map<MPRISProperty, struct sd_null_term_str> MPRISPropertyNames =
  {{MPRISProperty::Volume, {"Volume", NULL}},
   {MPRISProperty::PlaybackStatus, {"PlaybackStatus", NULL}},
   {MPRISProperty::LoopStatus, {"LoopStatus", NULL}},
   {MPRISProperty::Shuffle, {"Shuffle", NULL}},
   {MPRISProperty::Metadata, {"Metadata", NULL}}};

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

static std::string GetMetadataString(ITrack* track, const char* key)
{
  track->GetString(key, localBuffer, sizeof(localBuffer));
  return std::string(localBuffer);
}

static class MPRISRemote : public IPlaybackRemote {
    private:
        IPlaybackService* playback;
        sd_bus* bus;
        std::mutex sd_mutex;
        std::shared_ptr<std::thread> thread;
        bool mpris_initialized;
        bool stop_processing;

        bool MPRISInit();
        void MPRISDeinit();
        void MPRISEmitChange(MPRISProperty prop) {
          if (bus) {
            char** strv = (char**)(&MPRISPropertyNames.at(prop));
            std::unique_lock<decltype(sd_mutex)> lock(sd_mutex);
            sd_bus_emit_properties_changed_strv(bus, "/org/mpris/MediaPlayer2",
                                                "org.mpris.MediaPlayer2.Player",
                                                strv);
            sd_bus_flush(bus);
          }
        };

       void MPRISEmitSeek(double curpos) {
         if (bus) {
           int64_t position = (int64_t)(curpos*1000*1000);
           std::unique_lock<decltype(sd_mutex)> lock(sd_mutex);
           sd_bus_emit_signal(bus, "/org/mpris/MediaPlayer2",
                              "org.mpris.MediaPlayer2.Player",
                              "Seeked", "x", position);
           }
       };

      void MPRISLoop() {
        while (!stop_processing) {
          if (bus) {
            std::unique_lock<decltype(sd_mutex)> lock(sd_mutex);
            while(sd_bus_process(bus, NULL) > 0);
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      }

    public:
        MPRISRemote() {
          playback = NULL;
          bus = NULL;
          stop_processing = false;
          mpris_initialized = false;
        };
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

        virtual void OnTrackChanged(ITrack* track) {
          if (playback) {
            MPRISEmitChange(MPRISProperty::Metadata);
            MPRISEmitSeek(playback->GetPosition());
          }
        }

        virtual void OnPlaybackStateChanged(PlaybackState state) {
          if (playback) {
            MPRISEmitChange(MPRISProperty::PlaybackStatus);
          }
        }

        virtual void OnVolumeChanged(double volume) {
          if (playback) {
            MPRISEmitChange(MPRISProperty::Volume);
          }
        }

        virtual void OnPlaybackTimeChanged(double time) {
          if (playback) {
            MPRISEmitChange(MPRISProperty::Metadata);
            MPRISEmitSeek(time);
          }
        }

        virtual void OnModeChanged(RepeatMode repeatMode, bool shuffled) {
          if (playback) {
            MPRISEmitChange(MPRISProperty::LoopStatus);
            MPRISEmitChange(MPRISProperty::Shuffle);
          }
        }

        virtual void OnPlayQueueChanged() {
        }

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

     struct MPRISMetadataValues MPRISGetMetadata() {
       struct MPRISMetadataValues metadata;
       if (playback) {
         auto curTrack = playback->GetPlayingTrack();
         if (curTrack) {
           metadata.artist = GetMetadataString(curTrack, track::Artist);
           metadata.title = GetMetadataString(curTrack, track::Title);
           metadata.albumArtist = GetMetadataString(curTrack, track::AlbumArtist);
           metadata.genre = GetMetadataString(curTrack, track::Genre);
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

} remote;

static class MPRISPlugin : public IPlugin {
public:
  MPRISPlugin() {
  }

  virtual void Release() { }
  virtual const char* Name() { return "MPRIS interface"; }
  virtual const char* Version() { return "0.1.0"; }
  virtual const char* Author() { return "brunosmmm"; }
  virtual const char* Guid() { return "457df67f-f489-415f-975e-282f470b1c10"; }
  virtual bool Configurable() { return false; }
  virtual void Configure() { }
  virtual void Reload() {  }
  virtual int SdkVersion() { return musik::core::sdk::SdkVersion; }
} plugin;

extern "C" IPlugin* GetPlugin() {
  return &plugin;
}

extern "C" IPlaybackRemote* GetPlaybackRemote() {
  return &remote;
}

//wrappers
static int next_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
  remote.MPRISNext();
  return sd_bus_reply_method_return(m, "");
}

static int prev_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
  remote.MPRISPrev();
  return sd_bus_reply_method_return(m, "");
}

static int pause_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
  remote.MPRISPause();
  return sd_bus_reply_method_return(m, "");
}

static int playpause_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
  remote.MPRISPlayPause();
  return sd_bus_reply_method_return(m, "");
}

static int stop_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
  remote.MPRISStop();
  return sd_bus_reply_method_return(m, "");
}

static int play_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
  remote.MPRISPlay();
  return sd_bus_reply_method_return(m, "");
}

static int seek_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
  int64_t pos = 0;
  int ret = sd_bus_message_read_basic(m, 'x', &pos);
  if (ret < 0) {
    return ret;
  }
  remote.MPRISSeek(pos, true);
  return sd_bus_reply_method_return(m, "");
}

static int get_playback_status(sd_bus* bus, const char* path, const char *iface,
                               const char* prop, sd_bus_message* reply,
                               void* data, sd_bus_error* err) {
  const char* state = remote.MPRISGetPlaybackStatus();
  return sd_bus_message_append_basic(reply, 's', state);
}

static int get_loop_status(sd_bus* bus, const char* path, const char *iface,
                           const char* prop, sd_bus_message* reply,
                           void* data, sd_bus_error* err)  {
  const char* state = remote.MPRISGetLoopStatus();
  return sd_bus_message_append_basic(reply, 's', state);

}

static int set_loop_status(sd_bus* bus, const char* path, const char *iface,
                             const char* prop, sd_bus_message* value,
                             void* data, sd_bus_error* err)  {
  const char* _value = NULL;
  int ret = sd_bus_message_read_basic(value, 's', &_value);
  if (ret < 0) {
    return ret;
  }
  remote.MPRISSetLoopStatus(_value);
  return sd_bus_reply_method_return(value, "");
}

static int get_position(sd_bus* bus, const char* path, const char *iface,
                        const char* prop, sd_bus_message* reply,
                        void* data, sd_bus_error* err)  {
  const uint64_t pos = remote.MPRISGetPosition();
  return sd_bus_message_append_basic(reply, 'x', &pos);

}


static int get_shuffle_status(sd_bus* bus, const char* path, const char *iface,
                              const char* prop, sd_bus_message* reply,
                              void* data, sd_bus_error* err)  {
  const unsigned int state = remote.MPRISGetShuffleStatus();
  return sd_bus_message_append_basic(reply, 'b', &state);

}

static int set_shuffle_status(sd_bus* bus, const char* path, const char *iface,
                              const char* prop, sd_bus_message* value,
                              void* data, sd_bus_error* err)  {
  unsigned int _value = 0;
  int ret = sd_bus_message_read_basic(value, 'b', &_value);
  if (ret < 0) {
    return ret;
  }
  remote.MPRISSetShuffleStatus(_value);
  return sd_bus_reply_method_return(value, "");
}

static int get_volume(sd_bus* bus, const char* path, const char *iface,
                      const char* prop, sd_bus_message* reply,
                      void* data, sd_bus_error* err)  {
  double vol = remote.MPRISGetVolume();
  return sd_bus_message_append_basic(reply, 'd', &vol);

}

static int set_volume(sd_bus* bus, const char* path, const char *iface,
                      const char* prop, sd_bus_message* value,
                      void* data, sd_bus_error* err)  {
  double _value = 0;
  int ret = sd_bus_message_read_basic(value, 'd', &_value);
  if (ret < 0) {
    return ret;
  }
  if (_value < 0.0) {
    _value = 0.0;
  } else if (_value > 1.0) {
    _value = 1.0;
  }
  remote.MPRISSetVolume(_value);
  return sd_bus_reply_method_return(value, "");
}

static void sd_msg_append_dict(sd_bus_message* msg, const char* key, const void* value, char type) {
  const char type_str[] = {type, 0};
  sd_bus_message_open_container(msg, 'e', "sv");
  sd_bus_message_append_basic(msg, 's', key);
  sd_bus_message_open_container(msg, 'v', type_str);
  sd_bus_message_append_basic(msg, type, value);
  sd_bus_message_close_container(msg);
  sd_bus_message_close_container(msg);
}

static void sd_msg_append_strlist_dict(sd_bus_message* msg, const char* key, const void* value) {
  sd_bus_message_open_container(msg, 'e', "sv");
  sd_bus_message_append_basic(msg, 's', key);
  sd_bus_message_open_container(msg, 'v', "as");
  sd_bus_message_open_container(msg, 'a', "s");
  sd_bus_message_append_basic(msg, 's', value);
  sd_bus_message_close_container(msg);
  sd_bus_message_close_container(msg);
  sd_bus_message_close_container(msg);
}

static int get_metadata(sd_bus* bus, const char* path, const char *iface,
                        const char* prop, sd_bus_message* reply,
                        void* data, sd_bus_error* err)  {
  int ret = 0;
  struct MPRISMetadataValues metadata = remote.MPRISGetMetadata();

  ret = sd_bus_message_open_container(reply, 'a', "{sv}");
  if (ret < 0) {
    return ret;
  }

  if (metadata.available) {

    // append fields
    sd_msg_append_dict(reply, "mpris:trackid", metadata.trackid.c_str(), 'o');
    sd_msg_append_dict(reply, "mpris:length", &metadata.length, 'x');
    sd_msg_append_strlist_dict(reply, "xesam:artist", metadata.artist.c_str());
    sd_msg_append_dict(reply, "xesam:title", metadata.title.c_str(), 's');
    sd_msg_append_dict(reply, "xesam:album", metadata.album.c_str(), 's');
    sd_msg_append_strlist_dict(reply, "xesam:albumArtist", metadata.albumArtist.c_str());
    sd_msg_append_strlist_dict(reply, "xesam:genre", metadata.genre.c_str());
    sd_msg_append_dict(reply, "xesam:trackNumber", &metadata.trackNumber, 'i');
    sd_msg_append_dict(reply, "xesam:discNumber", &metadata.discNumber, 'i');
    sd_msg_append_strlist_dict(reply, "xesam:comment", metadata.comment.c_str());


  }
  ret = sd_bus_message_close_container(reply);
  if (ret < 0) {
    return ret;
  }
  return 0;
}

static int sd_response_true(sd_bus* bus, const char* path, const char* iface,
                            const char* prop, sd_bus_message *reply, void* data,
                            sd_bus_error* err)
{
  const unsigned int resp = 1;
  return sd_bus_message_append_basic(reply, 'b', &resp);
}

static int sd_response_false(sd_bus* bus, const char* path, const char* iface,
                             const char* prop, sd_bus_message *reply, void* data,
                             sd_bus_error* err)
{
  const unsigned int resp = 0;
  return sd_bus_message_append_basic(reply, 'b', &resp);
}


static int sd_write_nop(sd_bus* bus, const char* path, const char* iface,
                        const char* prop, sd_bus_message *reply, void* data,
                        sd_bus_error* err)
{
  return sd_bus_reply_method_return(reply, "");
}

static int sd_method_nop(sd_bus_message* m, void* data, sd_bus_error* err)
{
  return sd_bus_reply_method_return(m, "");
}


static int sd_response_rate(sd_bus* bus, const char* path, const char* iface,
                            const char* prop, sd_bus_message *reply, void* data,
                            sd_bus_error* err)
{
  const double resp = 1.0;
  return sd_bus_message_append_basic(reply, 'd', &resp);
}


static int sd_response_id(sd_bus* bus, const char* path, const char* iface,
                          const char* prop, sd_bus_message *reply, void* data,
                          sd_bus_error* err)
{
  const char* identity = "musikcube";
  return sd_bus_message_append_basic(reply, 's', identity);
}

static int sd_response_urischemes(sd_bus* bus, const char* path, const char* iface,
                                  const char* prop, sd_bus_message *reply, void* data,
                                  sd_bus_error* err)
{
  const char* schemes[] = {"file", NULL};
  return sd_bus_message_append_strv(reply, (char**)schemes);
}

static int sd_response_mimetypes(sd_bus* bus, const char* path, const char* iface,
                                 const char* prop, sd_bus_message *reply, void* data,
                                 sd_bus_error* err)
{
  const char* mime[] = {NULL};
  return sd_bus_message_append_strv(reply, (char**)mime);
}

static const sd_bus_vtable musikcube_mp_table[] = {
  SD_BUS_VTABLE_START(0),
  SD_BUS_PROPERTY("CanQuit", "b", sd_response_false, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanRaise", "b", sd_response_false, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("HasTrackList", "b", sd_response_false, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("Identity", "s", sd_response_id, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("SupportedUriSchemes", "as", sd_response_urischemes, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("SupportedMimeTypes", "as", sd_response_mimetypes, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_METHOD("Raise", "", "", sd_method_nop, 0),
  SD_BUS_METHOD("Quit", "", "", sd_method_nop, 0),
  SD_BUS_VTABLE_END
};

static const sd_bus_vtable musikcube_mpp_table[] = {
  SD_BUS_VTABLE_START(0),
  SD_BUS_METHOD("Next", "", "", next_wrapper, 0),
	SD_BUS_METHOD("Previous", "", "", prev_wrapper, 0),
	SD_BUS_METHOD("Pause", "", "", pause_wrapper, 0),
	SD_BUS_METHOD("PlayPause", "", "", playpause_wrapper, 0),
	SD_BUS_METHOD("Stop", "", "", stop_wrapper, 0),
	SD_BUS_METHOD("Play", "", "", play_wrapper, 0),
	SD_BUS_METHOD("Seek", "x", "", seek_wrapper, 0),
  SD_BUS_PROPERTY("PlaybackStatus", "s", get_playback_status,
                  0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_WRITABLE_PROPERTY("LoopStatus", "s", get_loop_status,
                           set_loop_status, 0,
                           SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_WRITABLE_PROPERTY("Shuffle", "b", get_shuffle_status,
                           set_shuffle_status, 0,
                           SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("Position", "x", get_position, 0, 0),
  SD_BUS_WRITABLE_PROPERTY("Volume", "d", get_volume, set_volume,
                           0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("Metadata", "a{sv}", get_metadata, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanGoNext", "b", sd_response_true, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanGoPrevious", "b", sd_response_true, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanPlay", "b", sd_response_true, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanPause", "b", sd_response_true, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanSeek", "b", sd_response_true, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanControl", "b", sd_response_true, 0, 0),
  SD_BUS_PROPERTY("MinimumRate", "d", sd_response_rate, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("MaximumRate", "d", sd_response_rate, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_WRITABLE_PROPERTY("Rate", "d", sd_response_rate, sd_write_nop, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
	SD_BUS_METHOD("SetPosition", "ox", "", sd_method_nop, 0), // unimplemented
	SD_BUS_METHOD("OpenUri", "s", "", sd_method_nop, 0), // unimplemented
	SD_BUS_SIGNAL("Seeked", "x", 0),
  SD_BUS_VTABLE_END
};


bool MPRISRemote::MPRISInit() {
  int ret = 0;

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
                                 "org.mpris.MediaPlayer2", musikcube_mp_table, NULL);
  if (ret < 0) {
    MPRISDeinit();
    return false;
  }
  ret = sd_bus_add_object_vtable(this->bus, NULL, "/org/mpris/MediaPlayer2",
                                 "org.mpris.MediaPlayer2.Player", musikcube_mpp_table, NULL);
  if (ret < 0) {
    MPRISDeinit();
    return false;
  }

  ret = sd_bus_request_name(this->bus, "org.mpris.MediaPlayer2.musikcube", 0);
  if (ret < 0) {
    MPRISDeinit();
    return false;
  }

  // start loop
  thread.reset(new std::thread(std::bind(&MPRISRemote::MPRISLoop, this)));

  return true;
}

void MPRISRemote::MPRISDeinit() {
  if (this->bus) {
    sd_bus_unref(this->bus);
    bus = NULL;
  }
  stop_processing = true;
  if (thread) {
    thread->join();
    thread.reset();
  }
}
