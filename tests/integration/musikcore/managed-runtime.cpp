#include <musikcore/musikcore.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <random>
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

    bool checkLocaleIntegers(const std::string& originalDirectory) {
        namespace fs = std::filesystem;
        const auto directory = fs::u8path(ReadPath(MC_PATH_DATA)) / "test-locales";
        fs::create_directories(directory);
        std::ofstream(directory / "en_US.json") <<
            R"({"integers":{"limit":10,"fallback":12,"overflow":13,"wrong_type":14}})";
        std::ofstream(directory / "fixture.json") <<
            R"({"integers":{"limit":-3,"overflow":18446744073709551615,"wrong_type":2.5}})";

        constexpr mc_string_view missing { "missing", sizeof("missing") - 1 };
        int32_t unchanged = 17;
        const bool passed = LoadLocales(directory.string()) == MC_OK &&
            SelectLocale("fixture") == MC_OK &&
            LocaleInteger("limit", 0) == -3 &&
            LocaleInteger("fallback", 0) == 12 &&
            LocaleInteger("overflow", 0) == 13 &&
            LocaleInteger("wrong_type", 0) == 14 &&
            mc_locale_read_integer(missing, &unchanged) == MC_ERROR_NOT_FOUND && unchanged == 17;

        const bool restored = SelectLocale("en_US") == MC_OK && LoadLocales(originalDirectory) == MC_OK;
        return passed && restored;
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
        const auto keys = session.Keys();
        return session.GetBool(enabled) && session.GetDouble(gain) == 10.25 && !settings.GetBool(enabled) &&
            std::ranges::find(keys, name) != keys.end() && session.Save() == MC_OK;
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

    bool configureLoopbackAudio(std::string_view format) {
        auto configuration = ReadServerConfiguration();
        if (!configuration) {
            return false;
        }

        /* Ask the kernel for distinct ports so tests can run concurrently. */
        asio::io_context sockets;
        using Tcp = asio::ip::tcp;
        const Tcp::endpoint loopback(asio::ip::address_v4::loopback(), 0);
        Tcp::acceptor metadata(sockets, loopback);
        Tcp::acceptor audio(sockets, loopback);
        configuration->metadataPort = metadata.local_endpoint().port();
        configuration->audioPort = audio.local_endpoint().port();
        configuration->metadataEnabled = true;
        configuration->audioEnabled = true;
        configuration->password = std::to_string(std::random_device{}());
        metadata.close();
        audio.close();

        auto settings = Preferences { MC_PREFERENCE_SETTINGS };
        if (ConfigureServer(*configuration) != MC_OK) {
            return false;
        }
        settings.Write({
            Preference(MC_PREFERENCE_KEY_REMOTE_LIBRARY_HOSTNAME, "127.0.0.1"),
            Preference(MC_PREFERENCE_KEY_REMOTE_LIBRARY_WSS_PORT, static_cast<int32_t>(configuration->metadataPort)),
            Preference(MC_PREFERENCE_KEY_REMOTE_LIBRARY_HTTP_PORT, static_cast<int32_t>(configuration->audioPort)),
            Preference(MC_PREFERENCE_KEY_REMOTE_LIBRARY_PASSWORD, configuration->password),
            Preference(MC_PREFERENCE_KEY_REMOTE_LIBRARY_VIEWED, true),
            Preference(MC_PREFERENCE_KEY_REMOTE_LIBRARY_TRANSCODER_ENABLED, !format.empty()),
            Preference(MC_PREFERENCE_KEY_REMOTE_LIBRARY_TRANSCODER_BITRATE, format.empty() ? 0 : 128),
            Preference(MC_PREFERENCE_KEY_REMOTE_LIBRARY_TRANSCODER_FORMAT, format)
        });
        return true;
    }

    bool checkRemoteAudio(Context& context, Host& host, Playback& playback) {
        if (context.SelectLibrary(MC_LIBRARY_REMOTE) != MC_OK ||
            !waitFor(context, host, [&] { return context.GetLibraryStatus().state == MC_LIBRARY_CONNECTED; }))
        {
            std::fputs("Could not connect to the local server plugin\n", stderr);
            return false;
        }

        TrackResult remoteTrack;
        std::optional<mc_result> opened;
        OpenTrackSearch(context, TrackSearch { "Morning Light" },
            [&](mc_result status, TrackResult result, std::string_view) {
                opened = status;
                remoteTrack = std::move(result);
            });
        if (!waitFor(context, host, [&] { return opened.has_value(); }) ||
            opened != MC_OK || remoteTrack.Size() != 1 || playback.Play(remoteTrack, 0) != MC_OK)
        {
            std::fputs("Could not start the remote track\n", stderr);
            return false;
        }

        /* Position advances only when decoded audio reaches the Null output. */
        if (!waitFor(context, host, [&] {
            const auto snapshot = playback.Snapshot();
            return snapshot && snapshot->state == MC_PLAYBACK_PLAYING && snapshot->position > 0;
        })) {
            std::fputs("Remote HTTP audio did not reach the headless output\n", stderr);
            return false;
        }

        const auto metadata = playback.CurrentTrackMetadata();
        if (!metadata || metadata->Id() != remoteTrack.Id(0) ||
            metadata->Title().find("Morning Light") == std::string_view::npos)
        {
            std::fputs("Playback did not retain the remote track's loaded metadata\n", stderr);
            return false;
        }
        return true;
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

    struct LogProbe {
        static constexpr char tag[] = "managed-runtime-fixture";
        static constexpr char text[] = "worker emitted diagnostics";
        const std::thread::id owner = std::this_thread::get_id();
        std::atomic<bool> received { false };
        std::atomic<bool> correctThread { false };

        static void MC_CALL Receive(void* data, mc_log_level level, mc_string_view tag, mc_string_view text) {
            auto& probe = *static_cast<LogProbe*>(data);
            if (level == MC_LOG_WARNING && AsStringView(tag) == LogProbe::tag &&
                AsStringView(text) == LogProbe::text)
            {
                probe.correctThread.store(std::this_thread::get_id() == probe.owner);
                probe.received.store(true);
            }
        }
    };

    bool checkRuntime(
        const mc_context_options& options, Host& host, std::optional<Library>& retained, bool equalizer, bool remoteAudio, LogProbe& log)
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
        std::thread producer([] {
            WriteLog(MC_LOG_WARNING, LogProbe::tag, LogProbe::text);
        });
        producer.join();
        if (log.received.load() || !waitFor(context, host, [&] { return log.received.load(); }) ||
            !log.correctThread.load())
        {
            std::fputs("Diagnostics were not delivered on the dispatcher thread\n", stderr);
            return false;
        }

        /* The isolated fixture has no online credentials. It must complete
           through the dispatcher without attempting a network request. */
        Preferences { MC_PREFERENCE_SETTINGS }.SetString(MC_PREFERENCE_KEY_AUDDIO_API_TOKEN, "");
        std::optional<mc_result> lyricsStatus;
        const auto lyricsRequest = FindLyrics(context, "Fixture artist", "Fixture title",
            [&](mc_result status, std::string_view text) {
                lyricsStatus = text.empty() ? status : MC_ERROR_INTERNAL;
            });
        if (!lyricsRequest || lyricsStatus ||
            !waitFor(context, host, [&] { return lyricsStatus.has_value(); }) ||
            *lyricsStatus != MC_ERROR_NOT_FOUND ||
            ReadLastFmAccount() || UnlinkLastFmAccount() != MC_OK || ReadLastFmAccount())
        {
            std::fputs("Online services did not handle an unconfigured account\n", stderr);
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

        mc_indexer_state scannerState = MC_INDEXER_IDLE;
        const auto scannerStatus = mc_indexer_get_state(context.Get(), &scannerState);
        if (scannerStatus != ((context.Services() & MC_SERVICE_INDEXER) ? MC_OK : MC_ERROR_UNSUPPORTED)) {
            std::fputs("Indexer availability did not match the selected services\n", stderr);
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

        mc_result offThread = MC_OK;
        std::thread scheduler([&] { offThread = Indexer(context).Schedule(MC_INDEXER_SCAN_LOCAL); });
        scheduler.join();
        if (offThread != MC_ERROR_WRONG_THREAD) {
            std::fputs("Indexer admitted scheduling off the dispatcher thread\n", stderr);
            return false;
        }

        if (remoteAudio) {
            if (!checkRemoteAudio(context, host, playback)) {
                return false;
            }
            const auto plugins = ListPlugins();
            const auto server = std::find_if(plugins.begin(), plugins.end(), [](const auto& plugin) {
                return plugin.filename.starts_with("libserver.");
            });
            if (server == plugins.end() || SetPluginEnabled(server->filename, false) != MC_OK) {
                return false;
            }
        }
        else if (playback.Play(tracks, 0) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PLAYING); }))
        {
            return false;
        }

        if (context.Shutdown() != MC_OK || playback.Control(MC_PLAYBACK_STOP) == MC_OK ||
            Indexer(context).Schedule(MC_INDEXER_SCAN_LOCAL) != MC_ERROR_BUSY)
        {
            std::fputs("Shut-down runtime still admitted work\n", stderr);
            return false;
        }

        if (remoteAudio) {
            const auto port = Preferences { MC_PREFERENCE_SETTINGS }.GetInt(MC_PREFERENCE_KEY_REMOTE_LIBRARY_WSS_PORT);
            asio::io_context sockets;
            asio::ip::tcp::socket socket(sockets);
            asio::error_code error;
            socket.connect({ asio::ip::address_v4::loopback(), static_cast<uint16_t>(port) }, error);
            if (error != asio::error::connection_refused) {
                std::fputs("Disabled server still listened after shutdown\n", stderr);
                return false;
            }
        }
        return true;
    }
}

int main(int argc, char** argv) {
    if (!std::filesystem::is_directory(std::filesystem::u8path(ReadPath(MC_PATH_APPLICATION))) ||
        !std::filesystem::is_directory(std::filesystem::u8path(ReadPath(MC_PATH_DATA))) ||
        mc_path_read(MC_PATH_HOME, nullptr, nullptr) != MC_ERROR_INVALID_ARGUMENT ||
        !ReadPath(UINT32_MAX).empty())
    {
        std::fprintf(stderr, "public path contract failed\n");
        return 1;
    }

    const auto localeDirectory = ReadPath(MC_PATH_APPLICATION) + "/locales";
    const auto loaded = LoadLocales(localeDirectory);
    const auto locales = ReadLocales();
    if (loaded != MC_OK || std::ranges::find(locales, "en_US") == locales.end() ||
        ReadSelectedLocale() != "en_US" ||
        SelectLocale("unavailable-locale") != MC_ERROR_NOT_FOUND ||
        Translate("settings_selected_locale") == "settings_selected_locale" ||
        Translate("missing-fixture-key") != "missing-fixture-key" ||
        !checkLocaleIntegers(localeDirectory))
    {
        std::fprintf(stderr, "public locale contract failed\n");
        return 1;
    }

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
    bool remoteAudio = false;
    bool playbackOnly = false;
    std::string_view transcodeFormat;
    for (int i = 1; i < argc; ++i) {
        const std::string_view option(argv[i]);
        equalizer |= option == "--check-equalizer";
        server |= option == "--check-server";
        remoteAudio |= option == "--check-remote-audio";
        playbackOnly |= option == "--playback-only";
        constexpr std::string_view transcodeOption = "--transcode=";
        if (option.starts_with(transcodeOption)) {
            transcodeFormat = option.substr(transcodeOption.size());
        }
    }
    /* Persist disabled listeners before starting the isolated plugin runtime. */
    if (!checkServerConfiguration(server)) {
        std::fputs("Server configuration did not round trip through the public API\n", stderr);
        return 1;
    }

    if (remoteAudio && !configureLoopbackAudio(transcodeFormat)) {
        std::fputs("Could not configure the loopback server\n", stderr);
        return 1;
    }

    LogProbe log;
    auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    /* reuse the real decoder/output lifecycle contract without a scanner.
    this catches accidental Indexer dependencies during playback or teardown. */
    options.services = playbackOnly ? MC_SERVICE_PLAYBACK : MC_SERVICE_ALL;
    options.flags = MC_CONTEXT_ENABLE_LOGGING;
    options.log = LogProbe::Receive;
    options.log_user_data = &log;
    options.wakeup = Host::Wake;
    options.wakeup_user_data = &host;
    std::optional<Library> oldLibrary;
    if (!checkRuntime(options, host, oldLibrary, equalizer, remoteAudio, log)) {
        return 1;
    }

    if (remoteAudio) {
        std::ifstream logFile(std::filesystem::u8path(ReadPath(MC_PATH_DATA) + "log.txt"));
        const std::string recorded(std::istreambuf_iterator<char>(logFile), {});
        if (recorded.find("musikcore://remote-track/[redacted]") == std::string::npos ||
            recorded.find(R"("password":)") != std::string::npos)
        {
            std::fputs("Remote playback logs did not redact the URI payload\n", stderr);
            return 1;
        }
        return 0;
    }

    /* a retained catalog pins its closed backend safely across a restart with
    fewer services. saved scan roots must still report configured without an
    indexer, and catalog reads must work without a new provider session. */
    options.services = MC_SERVICE_LIBRARY;
    Context reopened(options);
    oldLibrary.reset();
    if (!reopened.GetLibrary() || !reopened.GetLibraryStatus().configured ||
        reopened.Services() != MC_SERVICE_LIBRARY)
    {
        return 1;
    }
    std::optional<mc_result> readStatus;
    OpenTrackSearch(reopened, TrackSearch { "Morning Light" },
        [&](mc_result status, TrackResult tracks, std::string_view) {
            readStatus = status == MC_OK && tracks.Size() ? MC_OK : MC_ERROR_INTERNAL;
        });
    const bool read = waitFor(reopened, host, [&] { return readStatus.has_value(); });
    const auto stopped = reopened.Shutdown();
    return read && readStatus == MC_OK && stopped == MC_OK ? 0 : 1;
}
