
#include "mpris.h"
#include "dbus.h"
#include <cerrno>
#include <cmath>

/* report rejected commands to the D-Bus client instead of pretending they ran. */
static int commandError(sd_bus_error* error, mc_result result) {
    if (result == MC_OK) {
        return 0;
    }
    const int code = result == MC_ERROR_INVALID_ARGUMENT ? EINVAL
        : result == MC_ERROR_QUEUE_FULL || result == MC_ERROR_IN_FLIGHT ? EBUSY : EIO;
    return sd_bus_error_set_errno(error, code);
}

static int next_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
    MPRISRemote* remote = (MPRISRemote*)data;
    const auto status = remote->MPRISNext();
    if (status != MC_OK) {
        return commandError(err, status);
    }
    return sd_bus_reply_method_return(m, "");
}

static int prev_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
    MPRISRemote* remote = (MPRISRemote*)data;
    const auto status = remote->MPRISPrev();
    if (status != MC_OK) {
        return commandError(err, status);
    }
    return sd_bus_reply_method_return(m, "");
}

static int pause_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
    MPRISRemote* remote = (MPRISRemote*)data;
    const auto status = remote->MPRISPause();
    if (status != MC_OK) {
        return commandError(err, status);
    }
    return sd_bus_reply_method_return(m, "");
}

static int playpause_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
    MPRISRemote* remote = (MPRISRemote*)data;
    const auto status = remote->MPRISPlayPause();
    if (status != MC_OK) {
        return commandError(err, status);
    }
    return sd_bus_reply_method_return(m, "");
}

static int stop_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
    MPRISRemote* remote = (MPRISRemote*)data;
    const auto status = remote->MPRISStop();
    if (status != MC_OK) {
        return commandError(err, status);
    }
    return sd_bus_reply_method_return(m, "");
}

static int play_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
    MPRISRemote* remote = (MPRISRemote*)data;
    const auto status = remote->MPRISPlay();
    if (status != MC_OK) {
        return commandError(err, status);
    }
    return sd_bus_reply_method_return(m, "");
}

static int seek_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
    MPRISRemote* remote = (MPRISRemote*)data;
    int64_t pos = 0;
    int ret = sd_bus_message_read_basic(m, 'x', &pos);
    if (ret < 0) {
        return ret;
    }
    const auto status = remote->MPRISSeek(pos, true);
    if (status != MC_OK) {
        return commandError(err, status);
    }
    return sd_bus_reply_method_return(m, "");
}

static int set_position_wrapper(sd_bus_message* m, void* data, sd_bus_error* err) {
  MPRISRemote* remote = (MPRISRemote*)data;
  const char* buf = nullptr;
  int64_t pos = 0;
  int ret = sd_bus_message_read(m, "ox", &buf, &pos);
  if (ret < 0) {
    return ret;
  }
  const auto status = remote->MPRISSeek(pos, false, buf);
  if (status != MC_OK) {
    return commandError(err, status);
  }
  return sd_bus_reply_method_return(m, "");
}

static int get_playback_status(sd_bus* bus, const char* path, const char *iface,
                               const char* prop, sd_bus_message* reply,
                               void* data, sd_bus_error* err) {
    MPRISRemote* remote = (MPRISRemote*)data;
    const char* state = remote->MPRISGetPlaybackStatus();
    return sd_bus_message_append_basic(reply, 's', state);
}

static int get_loop_status(sd_bus* bus, const char* path, const char *iface,
                           const char* prop, sd_bus_message* reply,
                           void* data, sd_bus_error* err)  {
    MPRISRemote* remote = (MPRISRemote*)data;
    const char* state = remote->MPRISGetLoopStatus();
    return sd_bus_message_append_basic(reply, 's', state);

}

static int set_loop_status(sd_bus* bus, const char* path, const char *iface,
                           const char* prop, sd_bus_message* value,
                           void* data, sd_bus_error* err)  {
    MPRISRemote* remote = (MPRISRemote*)data;
    const char* _value = NULL;
    int ret = sd_bus_message_read_basic(value, 's', &_value);
    if (ret < 0) {
        return ret;
    }
    const auto status = remote->MPRISSetLoopStatus(_value);
    return status == MC_OK ? 1 : commandError(err, status);
}

static int get_position(sd_bus* bus, const char* path, const char *iface,
                        const char* prop, sd_bus_message* reply,
                        void* data, sd_bus_error* err)  {
    MPRISRemote* remote = (MPRISRemote*)data;
    const uint64_t pos = remote->MPRISGetPosition();
    return sd_bus_message_append_basic(reply, 'x', &pos);

}


static int get_shuffle_status(sd_bus* bus, const char* path, const char *iface,
                              const char* prop, sd_bus_message* reply,
                              void* data, sd_bus_error* err)  {
    MPRISRemote* remote = (MPRISRemote*)data;
    const unsigned int state = remote->MPRISGetShuffleStatus();
    return sd_bus_message_append_basic(reply, 'b', &state);

}

static int set_shuffle_status(sd_bus* bus, const char* path, const char *iface,
                              const char* prop, sd_bus_message* value,
                              void* data, sd_bus_error* err)  {
    MPRISRemote* remote = (MPRISRemote*)data;
    unsigned int _value = 0;
    int ret = sd_bus_message_read_basic(value, 'b', &_value);
    if (ret < 0) {
        return ret;
    }
    const auto status = remote->MPRISSetShuffleStatus(_value);
    return status == MC_OK ? 1 : commandError(err, status);
}

static int get_volume(sd_bus* bus, const char* path, const char *iface,
                      const char* prop, sd_bus_message* reply,
                      void* data, sd_bus_error* err)  {
    MPRISRemote* remote = (MPRISRemote*)data;
    double vol = remote->MPRISGetVolume();
    return sd_bus_message_append_basic(reply, 'd', &vol);

}

static int set_volume(sd_bus* bus, const char* path, const char *iface,
                      const char* prop, sd_bus_message* value,
                      void* data, sd_bus_error* err)  {
    MPRISRemote* remote = (MPRISRemote*)data;
    double _value = 0;
    int ret = sd_bus_message_read_basic(value, 'd', &_value);
    if (ret < 0) {
        return ret;
    }
    if (!std::isfinite(_value)) {
        return commandError(err, MC_ERROR_INVALID_ARGUMENT);
    }
    if (_value < 0.0) {
        _value = 0.0;
    } else if (_value > 1.0) {
        _value = 1.0;
    }
    const auto status = remote->MPRISSetVolume(_value);
    return status == MC_OK ? 1 : commandError(err, status);
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

static int get_metadata_impl(sd_bus* bus, const char* path, const char *iface,
                        const char* prop, sd_bus_message* reply,
                        void* data, sd_bus_error* err)  {
    MPRISRemote* remote = (MPRISRemote*)data;
    int ret = 0;
    struct MPRISMetadataValues metadata = remote->MPRISGetMetadata();

    ret = sd_bus_message_open_container(reply, 'a', "{sv}");
    if (ret < 0) {
        return ret;
    }

    if (metadata.available) {
        // append fields
        sd_msg_append_dict(reply, "mpris:trackid", metadata.trackid.c_str(), 'o');
        sd_msg_append_dict(reply, "mpris:length", &metadata.length, 'x');
        sd_msg_append_dict(reply, "mpris:artUrl", metadata.albumArt.c_str(), 's');
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

static int get_metadata(sd_bus* bus, const char* path, const char* iface,
    const char* prop, sd_bus_message* reply, void* data, sd_bus_error* err)
{
    try {
        return get_metadata_impl(bus, path, iface, prop, reply, data, err);
    }
    catch (...) {
        return -ENOMEM;
    }
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

const sd_bus_vtable musikcube_mp_table[] = {
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

const sd_bus_vtable musikcube_mpp_table[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Next", "", "", next_wrapper, 0),
    SD_BUS_METHOD("Previous", "", "", prev_wrapper, 0),
    SD_BUS_METHOD("Pause", "", "", pause_wrapper, 0),
    SD_BUS_METHOD("PlayPause", "", "", playpause_wrapper, 0),
    SD_BUS_METHOD("Stop", "", "", stop_wrapper, 0),
    SD_BUS_METHOD("Play", "", "", play_wrapper, 0),
    SD_BUS_METHOD("Seek", "x", "", seek_wrapper, 0),
    SD_BUS_PROPERTY("PlaybackStatus", "s", get_playback_status, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_WRITABLE_PROPERTY("LoopStatus", "s", get_loop_status, set_loop_status, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_WRITABLE_PROPERTY("Shuffle", "b", get_shuffle_status, set_shuffle_status, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Position", "x", get_position, 0, 0),
    SD_BUS_WRITABLE_PROPERTY("Volume", "d", get_volume, set_volume, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
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
    SD_BUS_METHOD("SetPosition", "ox", "", set_position_wrapper, 0),
    SD_BUS_METHOD("OpenUri", "s", "", sd_method_nop, 0),
    SD_BUS_SIGNAL("Seeked", "x", 0),
    SD_BUS_VTABLE_END
};
