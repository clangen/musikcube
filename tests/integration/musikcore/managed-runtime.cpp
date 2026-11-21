#include <musikcore/musikcore.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace {
    using namespace musik::core::api;
    using namespace std::chrono_literals;

    /* Owns the blocking host wakeup state. Neither copyable nor movable. */
    class Host {
    public:
        static void MC_CALL Wake(void* data) {
            auto& host = *static_cast<Host*>(data);
            std::lock_guard lock(host.mutex);
            ++host.generation;
            host.changed.notify_one();
        }

        size_t Generation() const {
            std::lock_guard lock(this->mutex);
            return this->generation;
        }

        bool WaitSince(size_t observed, std::chrono::steady_clock::time_point deadline) {
            std::unique_lock lock(this->mutex);
            return this->changed.wait_until(lock, deadline, [&] { return observed != this->generation; });
        }

    private:
        mutable std::mutex mutex;
        std::condition_variable changed;
        size_t generation = 0;
    };

    template<typename Predicate>
    bool waitFor(Context& context, Host& host, Predicate ready) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (std::chrono::steady_clock::now() < deadline) {
            const auto observed = host.Generation();
            if (context.Dispatch() != MC_OK) {
                return false;
            }
            if (ready()) {
                return true;
            }
            if (!host.WaitSince(observed, deadline)) {
                return false;
            }
        }
        return false;
    }

    bool checkPreferences() {
        constexpr char name[] = "RuntimeFixtureName";
        constexpr char length[] = "RuntimeFixtureLength";
        constexpr char enabled[] = "RuntimeFixtureEnabled";
        constexpr char gain[] = "RuntimeFixtureGain";
        Preferences session { MC_PREFERENCE_SESSION };
        if (session.GetInt(length, 3) != 3 || session.GetString(name, "default") != "default") {
            return false;
        }

        std::string text = "public";
        mc_preference_entry values[] = {
            { { name, sizeof(name) - 1 }, { MC_PREFERENCE_TEXT, {} } },
            { { length, sizeof(length) - 1 }, { MC_PREFERENCE_INTEGER, {} } }
        };
        values[0].value.data.text = { text.data(), text.size() };
        values[1].value.data.integer = 42;
        if (mc_preferences_write(MC_PREFERENCE_SESSION, values, std::size(values)) != MC_OK) {
            return false;
        }

        text[0] = 'X';
        values[1].value.type = UINT32_MAX;
        if (mc_preferences_write(MC_PREFERENCE_SESSION, values, std::size(values)) != MC_ERROR_INVALID_ARGUMENT ||
            session.GetString(name) != "public" || session.GetInt(length) != 42)
        {
            return false;
        }

        session.SetBool(enabled, true);
        session.SetDouble(gain, 10.25);
        const Preferences settings { MC_PREFERENCE_SETTINGS };
        return session.GetBool(enabled) && session.GetDouble(gain) == 10.25 && !settings.GetBool(enabled) &&
            session.Save() == MC_OK;
    }

    bool checkEqualizer(Context& context, Host& host) {
        const auto before = ReadEqualizer();
        if (!before) {
            return false;
        }

        unsigned updates = 0;
        if (SetEqualizerChangedCallback(context, [](void* data) {
            ++*static_cast<unsigned*>(data);
        }, &updates) != MC_OK)
        {
            return false;
        }

        auto gains = EqualizerGains {};
        gains.front() = 1.0;
        const bool updated = SetEqualizerEnabled(!before->enabled) == MC_OK &&
            SetEqualizerGains(gains) == MC_OK &&
            waitFor(context, host, [&] { return updates != 0; });
        const auto unsubscribed = SetEqualizerChangedCallback(context, nullptr);
        const auto after = ReadEqualizer();
        return updated && unsubscribed == MC_OK && updates == 1 && after &&
            before->enabled != after->enabled && after->bands[0].gain == gains.front();
    }

    bool checkServerConfiguration(bool expectedAvailable) {
        ServerConfiguration updated {
            false, false, false, false, MC_SERVER_DEFAULT_METADATA_PORT, MC_SERVER_DEFAULT_AUDIO_PORT,
            MC_SERVER_DEFAULT_CACHE_COUNT + 1, MC_SERVER_DEFAULT_ACTIVE_TRANSCODE_LIMIT, "runtime fixture"
        };
        if (!expectedAvailable) {
            return !ReadServerConfiguration() && ConfigureServer(updated) == MC_ERROR_UNSUPPORTED;
        }
        if (ConfigureServer(updated) != MC_OK) {
            return false;
        }

        const auto actual = ReadServerConfiguration();
        if (!actual || actual->metadataEnabled || actual->audioEnabled ||
            actual->password != updated.password || actual->cacheCount != updated.cacheCount)
        {
            return false;
        }

        updated.metadataPort = 0;
        return ConfigureServer(updated) == MC_ERROR_INVALID_ARGUMENT;
    }

    bool checkPluginSettings() {
        static constexpr char nullPlugin[] = "Null IOutput";
        static constexpr char sampleRate[] = "default_sample_rate";
        constexpr int32_t configuredRate = 44100;

        const auto catalog = ListPlugins();
        const auto null = std::ranges::find_if(catalog, [](const auto& plugin) {
            return plugin.name == nullPlugin;
        });
        if (null == catalog.end() || !null->enabled ||
            SetPluginEnabled(null->filename, false) != MC_OK)
        {
            return false;
        }
        const auto disabled = ListPlugins();
        const auto disabledPlugin = std::ranges::find_if(disabled,
            [&](const auto& plugin) { return plugin.filename == null->filename; });
        if (disabledPlugin == disabled.end() || disabledPlugin->enabled ||
            SetPluginEnabled(null->filename, true) != MC_OK)
        {
            return false;
        }

        if (WritePluginSetting(null->id, sampleRate, configuredRate) != MC_OK ||
            WritePluginSetting(null->id, sampleRate, int32_t { -1 }) != MC_ERROR_INVALID_ARGUMENT)
        {
            return false;
        }
        const auto schema = ReadPluginSettings(null->id);
        if (!schema) {
            return false;
        }
        const auto setting = std::ranges::find_if(*schema, [](const auto& field) { return field.key == sampleRate; });
        return setting != schema->end() && std::holds_alternative<int32_t>(setting->value) &&
            std::get<int32_t>(setting->value) == configuredRate;
    }

    bool checkRuntime(
        const mc_context_options& options, Host& host, std::optional<Library>& retained, bool equalizer)
    {
        TrackResult tracks;
        mc_result complete = MC_ERROR_BUSY;
        bool wrongThread = false;
        const auto owner = std::this_thread::get_id();

        const auto beforeStartup = host.Generation();
        Context context(options);
        if (!host.WaitSince(beforeStartup, std::chrono::steady_clock::now() + 5s) ||
            context.GetLibraryStatus().kind != MC_LIBRARY_LOCAL)
        {
            std::fputs("Managed startup did not publish the local library and a wakeup\n", stderr);
            return false;
        }
        retained = context.GetLibrary();
        if (!retained || ReadOutputConfiguration().selectedDriver != "Null") {
            std::fputs("Managed startup did not load the catalog and headless output\n", stderr);
            return false;
        }
        if (equalizer && !checkEqualizer(context, host)) {
            std::fputs("Equalizer updates did not reach the public dispatcher\n", stderr);
            return false;
        }

        OpenTrackSearch(context, TrackSearch { "Morning Light" },
            [&](mc_result status, TrackResult result, std::string_view) {
                wrongThread = std::this_thread::get_id() != owner;
                complete = status;
                tracks = std::move(result);
            });
        if (!waitFor(context, host, [&] { return complete != MC_ERROR_BUSY; }) ||
            complete != MC_OK || wrongThread || !tracks.Size())
        {
            std::fputs("Managed search did not complete on the host thread\n", stderr);
            return false;
        }

        auto playback = context.GetPlayback();
        const auto inState = [&](mc_playback_state state) {
            const auto snapshot = playback.Snapshot();
            return snapshot && snapshot->state == state;
        };
        if (playback.Play(tracks, 0) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PLAYING); }))
        {
            std::fputs("Managed playback did not start\n", stderr);
            return false;
        }
        if (playback.Control(MC_PLAYBACK_TOGGLE_PAUSE) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PAUSED); }) ||
            playback.Control(MC_PLAYBACK_RELOAD_OUTPUT) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PREPARED); }))
        {
            std::fputs("Managed playback did not dispatch the delayed output reload\n", stderr);
            return false;
        }

        return context.Shutdown() == MC_OK;
    }
}

int main(int argc, char** argv) {
    if (!checkPreferences()) {
        std::fputs("Preference values did not round trip atomically through the public API\n", stderr);
        return 1;
    }

    Host host;
    if (!checkPluginSettings()) {
        std::fputs("Plugin metadata or schema setting did not round trip through the public API\n", stderr);
        return 1;
    }
    bool equalizer = false;
    bool server = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view option(argv[i]);
        equalizer |= option == "--check-equalizer";
        server |= option == "--check-server";
    }
    /* Persist disabled listeners before starting the isolated plugin runtime. */
    if (!checkServerConfiguration(server)) {
        std::fputs("Server configuration did not round trip through the public API\n", stderr);
        return 1;
    }

    auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    options.flags = MC_CONTEXT_MANAGED_RUNTIME;
    options.wakeup = Host::Wake;
    options.wakeup_user_data = &host;
    std::optional<Library> oldLibrary;
    if (!checkRuntime(options, host, oldLibrary, equalizer)) {
        return 1;
    }

    /* A retained catalog pins its closed backend safely across runtime restart. */
    Context reopened(options);
    oldLibrary.reset();
    return reopened.GetLibrary().has_value() ? 0 : 1;
}
