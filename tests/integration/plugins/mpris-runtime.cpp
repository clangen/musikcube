#include <musikcore/musikcore.hpp>
#include "../../support/host-wakeup.h"
#include SDBUS_HEADER
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <unistd.h>

using namespace musik::core::api;

static void require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "MPRIS runtime: %s\n", message);
        std::abort();
    }
}

struct Metadata {
    std::string title;
    std::string track;
    std::string artwork;
};

static Metadata readMetadata(sd_bus* bus, const std::string& name) {
    Metadata result;
    sd_bus_message* message = nullptr;
    if (sd_bus_get_property(bus, name.c_str(), "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player",
        "Metadata", nullptr, &message, "a{sv}") < 0)
    {
        return result;
    }
    if (sd_bus_message_enter_container(message, 'a', "{sv}") > 0) {
        while (sd_bus_message_enter_container(message, 'e', "sv") > 0) {
            const char* key = nullptr;
            if (sd_bus_message_read_basic(message, 's', &key) < 0 || !key) {
                break;
            }
            std::string* target = nullptr;
            const char* type = "s";
            if (std::strcmp(key, "xesam:title") == 0) {
                target = &result.title;
            }
            else if (std::strcmp(key, "mpris:artUrl") == 0) {
                target = &result.artwork;
            }
            else if (std::strcmp(key, "mpris:trackid") == 0) {
                target = &result.track;
                type = "o";
            }
            if (target && sd_bus_message_enter_container(message, 'v', type) > 0) {
                const char* value = nullptr;
                if (sd_bus_message_read_basic(message, type[0], &value) > 0 && value) {
                    *target = value;
                }
                sd_bus_message_exit_container(message);
            }
            else {
                sd_bus_message_skip(message, "v");
            }
            sd_bus_message_exit_container(message);
        }
    }
    sd_bus_message_unref(message);
    return result;
}

int main() {
    const std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> wake(
        test_host_event_create(), test_host_event_destroy);
    require(bool(wake), "create owner wakeup");
    auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_PLAYBACK;
    options.flags = MC_CONTEXT_START_LOCAL;
    options.wakeup = test_host_event_signal;
    options.wakeup_user_data = wake.get();
    Context context(options);
    auto playback = context.GetPlayback();
    sd_bus* bus = nullptr;
    require(sd_bus_open_user(&bus) >= 0, "open isolated D-Bus session");
    sd_bus_set_method_call_timeout(bus, 2000000);
    const auto name = std::string("org.mpris.MediaPlayer2.musikcube.instance") + std::to_string(getpid());
    const char* object = "/org/mpris/MediaPlayer2";
    const char* player = "org.mpris.MediaPlayer2.Player";
    const auto wait = [&](auto predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            require(context.Dispatch() == MC_OK, "dispatch owner work");
            if (predicate()) {
                return true;
            }
            test_host_event_wait_for(wake.get(), 50);
        }
        return false;
    };

    TrackResult tracks;
    std::optional<mc_result> searched;
    OpenTrackSearch(context, TrackSearch{ { "Fixture Artist" } }, [&](mc_result status, TrackResult result, std::string_view) {
        searched = status;
        tracks = std::move(result);
    });
    require(wait([&] { return searched.has_value(); }) && searched == MC_OK && tracks.Size() == 2,
        "find indexed tracks");
    require(playback.Play(tracks, 0) == MC_OK, "start track");
    Metadata first;
    require(wait([&] {
        first = readMetadata(bus, name);
        return first.title == "Morning Light" && first.artwork.starts_with("file://") && !first.track.empty();
    }), "D-Bus receives current title, identity, and artwork");
    const std::string firstArtwork = first.artwork.substr(7);
    {
        std::ifstream image(firstArtwork, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(image)), std::istreambuf_iterator<char>());
        require(bytes == std::string(40, 'a'), "artwork URL resolves to the track's image");
    }

    require(wait([&] { const auto state = playback.Snapshot(); return state && state->state == MC_PLAYBACK_PLAYING; }),
        "track reaches playing state");
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    require(sd_bus_call_method(bus, name.c_str(), object, player, "Pause", &error, &reply, "") >= 0,
        "submit pause from D-Bus");
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    require(wait([&] { const auto state = playback.Snapshot(); return state && state->state == MC_PLAYBACK_PAUSED; }),
        "pause reaches playback owner");
    reply = nullptr;
    require(sd_bus_call_method(bus, name.c_str(), object, player, "Pause", &error, &reply, "") >= 0,
        "submit repeated pause");
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    require(context.Dispatch() == MC_OK && playback.Snapshot()->state == MC_PLAYBACK_PAUSED,
        "repeated pause leaves playback paused");
    reply = nullptr;
    require(sd_bus_call_method(bus, name.c_str(), object, player, "Play", &error, &reply, "") >= 0,
        "submit play from D-Bus");
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    require(wait([&] { const auto state = playback.Snapshot(); return state && state->state == MC_PLAYBACK_PLAYING; }),
        "play resumes playback");
    reply = nullptr;
    require(sd_bus_call_method(bus, name.c_str(), object, player, "Play", &error, &reply, "") >= 0,
        "submit repeated play");
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    require(context.Dispatch() == MC_OK && playback.Snapshot()->state == MC_PLAYBACK_PLAYING,
        "repeated play leaves playback playing");
    require(sd_bus_set_property(bus, name.c_str(), object, player, "LoopStatus", &error, "s", "Track") >= 0,
        "submit repeat mode from D-Bus");
    sd_bus_error_free(&error);
    require(wait([&] { const auto state = playback.Snapshot(); return state && state->repeat == MC_PLAYBACK_REPEAT_TRACK; }),
        "repeat reaches requested mode");
    require(sd_bus_set_property(bus, name.c_str(), object, player, "Shuffle", &error, "b", 1) >= 0,
        "submit shuffle from D-Bus");
    sd_bus_error_free(&error);
    require(wait([&] { const auto state = playback.Snapshot(); return state && state->shuffled; }),
        "shuffle reaches requested state");

    require(context.Close() == MC_OK, "shutdown joins the D-Bus worker");
    require(!std::ifstream(firstArtwork), "shutdown removes the temporary artwork");
    sd_bus_flush_close_unref(bus);
}
