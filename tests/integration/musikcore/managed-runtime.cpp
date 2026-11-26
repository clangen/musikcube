#include <musikcore/musikcore.hpp>
#include <plugins/server/ServerPreferences.h>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <nlohmann/json.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace {
    namespace api = musik::core::api;
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

    using namespace musik::core::serverprefs;
    constexpr int MC_SERVER_DEFAULT_METADATA_PORT = DefaultMetadataPort;
    constexpr int MC_SERVER_DEFAULT_AUDIO_PORT = DefaultAudioPort;
    constexpr int MC_SERVER_DEFAULT_CACHE_COUNT = DefaultCacheCount;
    constexpr int MC_SERVER_DEFAULT_ACTIVE_TRANSCODE_LIMIT = DefaultActiveTranscodeLimit;

    struct ServerConfiguration {
        bool metadataEnabled;
        bool audioEnabled;
        bool ipv6;
        bool synchronousTranscoding;
        uint16_t metadataPort;
        uint16_t audioPort;
        int32_t cacheCount;
        int32_t activeTranscodeLimit;
        std::string password;
    };

    constexpr std::string_view serverPluginId = "9fc897a3-dfd5-4524-a0fc-b02f46aea4a9";

    std::optional<ServerConfiguration> ReadServerConfiguration() {
        const auto settings = api::ReadPluginSettings(serverPluginId);
        if (!settings) {
            return std::nullopt;
        }
        ServerConfiguration result { true, true, false, false, 7905, 7906, 50, 4, {} };
        for (const auto& field : *settings) {
            if (field.key == "websocket_server_enabled") {
                result.metadataEnabled = std::get<bool>(field.value);
            }
            else if (field.key == "http_server_enabled") {
                result.audioEnabled = std::get<bool>(field.value);
            }
            else if (field.key == "use_ipv6") {
                result.ipv6 = std::get<bool>(field.value);
            }
            else if (field.key == "transcoder_synchronous") {
                result.synchronousTranscoding = std::get<bool>(field.value);
            }
            else if (field.key == "websocket_server_port") {
                result.metadataPort = (uint16_t) std::get<int32_t>(field.value);
            }
            else if (field.key == "http_server_port") {
                result.audioPort = (uint16_t) std::get<int32_t>(field.value);
            }
            else if (field.key == "transcoder_cache_count") {
                result.cacheCount = std::get<int32_t>(field.value);
            }
            else if (field.key == "transcoder_max_active_count") {
                result.activeTranscodeLimit = std::get<int32_t>(field.value);
            }
            else if (field.key == "password") {
                result.password = std::get<std::string>(field.value);
            }
        }
        return result;
    }

    mc_result ConfigureServer(const ServerConfiguration& value) {
        const std::pair<std::string_view, api::PreferenceValue> fields[] {
            { "websocket_server_enabled", value.metadataEnabled },
            { "http_server_enabled", value.audioEnabled },
            { "use_ipv6", value.ipv6 },
            { "transcoder_synchronous", value.synchronousTranscoding },
            { "websocket_server_port", (int32_t) value.metadataPort },
            { "http_server_port", (int32_t) value.audioPort },
            { "transcoder_cache_count", value.cacheCount },
            { "transcoder_max_active_count", value.activeTranscodeLimit },
            { "password", value.password },
        };
        for (const auto& [key, setting] : fields) {
            const auto status = api::WritePluginSetting(serverPluginId, key, setting);
            if (status != MC_OK) {
                return status;
            }
        }
        const auto status = api::ApplyPluginSettings(serverPluginId);
        return status == MC_ERROR_UNSUPPORTED ? MC_OK : status;
    }

    bool checkServerConfiguration(bool expectedAvailable) {
        ServerConfiguration updated {
            false, false, false, false, MC_SERVER_DEFAULT_METADATA_PORT, MC_SERVER_DEFAULT_AUDIO_PORT,
            MC_SERVER_DEFAULT_CACHE_COUNT + 1, MC_SERVER_DEFAULT_ACTIVE_TRANSCODE_LIMIT, "runtime fixture"
        };
        if (!expectedAvailable) {
            return !ReadServerConfiguration();
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

        return WritePluginSetting(serverPluginId, "http_server_port", int32_t { 0 }) == MC_ERROR_INVALID_ARGUMENT;
    }

    /* both the real remote client and the direct HTTP probe authenticate to this
    isolated loopback server. the latter sends the base64 form of default:password. */
    constexpr char fixturePassword[] = "transcode-fixture";
    constexpr char fixtureBasic[] = "ZGVmYXVsdDp0cmFuc2NvZGUtZml4dHVyZQ==";

    bool configureLoopbackAudio(std::string_view format, bool synchronous, bool noTranscodeCache) {
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
        configuration->password = fixturePassword;
        configuration->synchronousTranscoding = synchronous;
        if (noTranscodeCache) {
            configuration->cacheCount = 0;
        }
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

    /* issue a real authenticated HTTP/1.0 request. closing the connection marks
    the end of an on-demand response whose encoded length is unknown. */
    std::pair<std::string, std::string> fetchAudio(
        uint16_t port, const std::string& path, std::string_view range = {}, std::string_view userAgent = {})
    {
        asio::ip::tcp::iostream client;
        client.expires_after(4s);
        client.connect("127.0.0.1", std::to_string(port));
        client << "GET " << path << " HTTP/1.0\r\nHost: localhost\r\nAuthorization: Basic " << fixtureBasic << "\r\n";
        if (!userAgent.empty()) {
            client << "User-Agent: " << userAgent << "\r\n";
        }
        if (!range.empty()) {
            client << "Range: " << range << "\r\n";
        }
        client << "\r\n" << std::flush;
        std::string response(std::istreambuf_iterator<char>(client), {});
        const auto end = response.find("\r\n\r\n");
        if (end == std::string::npos) {
            return {};
        }
        return { response.substr(0, end + 2), response.substr(end + 4) };
    }

    /* a generated MP3 has no exact length until flush. once cached, a new
    request for its last byte must return precisely that byte as a 206 response. */
    bool checkEncodedResponse(int64_t trackId, bool rangeFallback) {
        const auto config = ReadServerConfiguration();
        if (!config) {
            return false;
        }
        const auto [originalHeaders, wav] = fetchAudio(config->audioPort, "/audio/id/" + std::to_string(trackId));
        const std::string nameHeader = "X-musikcube-Filename-Override: ";
        const auto name = originalHeaders.find(nameHeader);
        if (name == std::string::npos || wav.compare(0, 4, "RIFF") != 0) {
            std::fputs("Original track did not supply its filename and WAV bytes\n", stderr);
            return false;
        }
        const auto start = name + nameHeader.size();
        const auto externalId = originalHeaders.substr(start, originalHeaders.find("\r\n", start) - start);
        const auto external = "/audio/external_id/" + externalId + "?format=mp3&bitrate=";
        /* musikdroid's proxy marks a download complete only at the advertised
        length. its first request already needs the finalized gapless header. */
        constexpr std::string_view androidAgent = "Dalvik/2.1.0 (Linux; U; Android 15)";
        const auto [androidHeaders, mp3] = fetchAudio(config->audioPort, external + "80", {}, androidAgent);
        if (androidHeaders.find(" 200 ") == std::string::npos || mp3.empty() ||
            androidHeaders.find("Content-Length: " + std::to_string(mp3.size()) + "\r\n") == std::string::npos ||
            mp3.substr(0, 512).find("Info") == std::string::npos)
        {
            std::fputs("Android proxy did not get a complete, gapless MP3 on its first request\n", stderr);
            return false;
        }
        const auto [androidRange, firstBytes] = fetchAudio(config->audioPort, external + "80", "bytes=0-255", androidAgent);
        if (androidRange.find(" 206 ") == std::string::npos ||
            androidRange.find("Content-Range: bytes 0-255/" + std::to_string(mp3.size()) + "\r\n") == std::string::npos ||
            firstBytes != mp3.substr(0, 256))
        {
            std::fputs("Android header range did not match the finalized first download\n", stderr);
            return false;
        }
        if (originalHeaders.find("Content-Type: audio/wav") == std::string::npos ||
            androidHeaders.find("Content-Type: audio/mpeg") == std::string::npos)
        {
            std::fputs("Audio response MIME does not match the original or encoded track\n", stderr);
            return false;
        }
        const auto [headers, bytes] = fetchAudio(config->audioPort, external + "112",
            rangeFallback ? "bytes=0-" : "");
        const bool knownLength = headers.find("\r\nContent-Length:") != std::string::npos;
        const bool refreshHeader = headers.find("X-musikcube-Estimated-Content-Length: true") != std::string::npos;
        if (headers.find(" 200 ") == std::string::npos || knownLength != config->synchronousTranscoding ||
            refreshHeader == knownLength || bytes.size() < 4096 || bytes.compare(0, 4, "RIFF") == 0)
        {
            std::fputs("MP3 must serve encoded bytes and report a length only after the file is complete\n", stderr);
            return false;
        }
        const auto last = std::to_string(bytes.size() - 1);
        const auto [cached, tail] = fetchAudio(config->audioPort, external + "112", "bytes=" + last + "-" + last);
        if (config->cacheCount || config->synchronousTranscoding || rangeFallback) {
            if (cached.find(" 206 ") == std::string::npos || tail.size() != 1 || tail.back() != bytes.back()) {
                std::fputs("Completed MP3 did not serve its final byte\n", stderr);
                return false;
            }
        }
        else if (cached.find(" 200 ") == std::string::npos || tail.size() != bytes.size()) {
            std::fputs("A live transcode with caching disabled unexpectedly retained a seekable file\n", stderr);
            return false;
        }
        /* choose a fresh bitrate so the cache cannot satisfy this seek. only a
        synchronous encode or an explicit fallback may turn it into a byte range. */
        const auto uncached = "/audio/id/" + std::to_string(trackId) + "?bitrate=96&format=mp3";
        const auto [rangeHeaders, rangeBytes] = fetchAudio(config->audioPort, uncached, "bytes=10-12");
        if (rangeFallback || config->synchronousTranscoding) {
            if (rangeHeaders.find(" 206 ") == std::string::npos ||
                rangeHeaders.find("Content-Range: bytes 10-12/") == std::string::npos || rangeBytes.size() != 3)
            {
                std::fputs("Configured range fallback did not serve the requested encoded bytes\n", stderr);
                return false;
            }
        }
        else if (rangeHeaders.find(" 200 ") == std::string::npos || rangeBytes.size() <= 3) {
            std::fputs("Default on-demand encode did not stream the whole response\n", stderr);
            return false;
        }
        return true;
    }

    /* the live queue can contain tracks whose metadata comes from this same
    server. its network worker must stay available to answer that nested read. */
    bool checkSelfRemoteQueue(Context& context, Host& host, Playback& playback, const TrackResult& tracks) {
        const auto expectedId = tracks.Id(0);
        for (size_t i = 0; i < 1000; ++i) {
            if (playback.InsertResult(tracks, MC_PLAYBACK_TRACK_APPEND) != MC_OK) {
                return false;
            }
        }
        const auto configuration = ReadServerConfiguration();
        if (!configuration) {
            return false;
        }
        const auto port = configuration->metadataPort;
        auto library = context.GetLibrary();
        std::optional<mc_result> saved;
        int64_t playlistId = 0;
        const auto awaitSaved = [&] {
            if (!waitFor(context, host, [&] { return saved.has_value(); })) {
                /* the accepted completion borrows saved and playlistId. a
                timeout must stop before these captures leave scope. */
                std::fputs("scope session playlist request exceeded its completion deadline\n", stderr);
                std::abort();
            }
            return saved == MC_OK;
        };
        if (!library) {
            return false;
        }
        SavePlaylist(context, *library, 0, "Scope session paging", { expectedId, expectedId, expectedId },
            [&](mc_result status, int64_t id, std::string_view) {
                saved = status;
                playlistId = id;
            });
        if (!awaitSaved() || playlistId <= 0) {
            return false;
        }
        std::atomic<bool> finished { false };
        std::atomic<bool> succeeded { false };
        std::thread probe([&] {
            using Client = websocketpp::client<websocketpp::config::asio_client>;
            using json = nlohmann::json;
            try {
                Client client;
                client.clear_access_channels(websocketpp::log::alevel::all);
                client.clear_error_channels(websocketpp::log::elevel::all);
                client.init_asio();
                asio::steady_timer deadline(client.get_io_context());
                deadline.expires_after(15s);
                std::string lastReply;
                const auto finish = [&](bool passed) {
                    if (!finished.load()) {
                        if (!passed) {
                            std::fprintf(stderr, "scope session probe failed after reply %s\n", lastReply.c_str());
                        }
                        succeeded.store(passed);
                        finished.store(true);
                        Host::Wake(&host);
                    }
                    deadline.cancel();
                    client.stop();
                };
                deadline.async_wait([&](const asio::error_code& error) {
                    if (!error) {
                        finish(false);
                    }
                });
                const auto send = [&](websocketpp::connection_hdl connection,
                    const char* name, const char* id, json options)
                {
                    const json request {
                        { "name", name },
                        { "type", "request" },
                        { "id", id },
                        { "device_id", "self-remote-queue-test" },
                        { "options", std::move(options) },
                    };
                    websocketpp::lib::error_code error;
                    client.send(connection, request.dump(), websocketpp::frame::opcode::text, error);
                    if (error) {
                        finish(false);
                    }
                };
                unsigned opened = 0, scopeEvents = 0;
                bool reconnecting = false;
                websocketpp::connection_hdl scopedConnection;
                const auto connectPeer = [&] {
                    websocketpp::lib::error_code error;
                    auto next = client.get_connection("ws://127.0.0.1:" + std::to_string(port), error);
                    if (error) {
                        finish(false);
                        return;
                    }
                    client.connect(next);
                };
                client.set_open_handler([&](auto connection) {
                    ++opened;
                    if (opened != 2) {
                        scopedConnection = connection;
                    }
                    const char* id = opened == 1 ? "auth" : opened == 2 ? "peer-auth" : "reconnect-auth";
                    send(connection, "authenticate", id, { { "password", fixturePassword } });
                });
                client.set_fail_handler([&](auto) { finish(false); });
                client.set_close_handler([&](auto connection) {
                    if (reconnecting && connection.lock() == scopedConnection.lock()) {
                        reconnecting = false;
                        connectPeer();
                    }
                    else {
                        finish(false);
                    }
                });
                client.set_message_handler([&](auto connection, auto message) {
                    try {
                        const auto response = json::parse(message->get_payload());
                        const auto id = response.value("id", std::string());
                        lastReply = id;
                        if (response.value("name", std::string()) == "library_scope_changed") {
                            if (connection.lock() != scopedConnection.lock() ||
                                response.value("type", std::string()) != "broadcast" || id.empty())
                            {
                                finish(false);
                                return;
                            }
                            ++scopeEvents;
                            return;
                        }
                        if (id == "auth") {
                            if (!response.at("options").value("authenticated", false)) {
                                finish(false);
                                return;
                            }
                            send(connection, "query_play_queue_tracks", "queue", {
                                { "type", "live" }, { "limit", -1 }, { "offset", 0 },
                            });
                        }
                        else if (id == "queue") {
                            const auto& rows = response.at("options").at("data");
                            bool found = false;
                            for (const auto& row : rows) {
                                found |= row.value("id", (int64_t) 0) == expectedId &&
                                    row.value("title", std::string()).find("Morning Light") != std::string::npos;
                            }
                            if (!found || rows.size() != 1000) {
                                finish(false);
                                return;
                            }
                            send(connection, "query_play_queue_tracks", "queue-next", {
                                { "type", "live" }, { "offset", 1000 },
                            });
                        }
                        else if (id == "queue-next") {
                            const auto& rows = response.at("options").at("data");
                            if (rows.size() != 1 || rows[0].value("id", (int64_t) 0) != expectedId) {
                                finish(false);
                                return;
                            }
                            send(connection, "list_categories", "categories", json::object());
                        }
                        else if (id == "categories") {
                            const auto& fields = response.at("options").at("data");
                            bool found = false;
                            for (const auto& field : fields) {
                                found |= field == "artist";
                            }
                            if (!found) {
                                finish(false);
                                return;
                            }
                            send(connection, "set_library_scope", "empty-scope", { { "sources", json::array() } });
                        }
                        else if (id == "empty-scope") {
                            if (response.at("options").at("sources") != json::array()) {
                                finish(false);
                                return;
                            }
                            send(connection, "set_library_scope", "unchanged-scope", { { "sources", json::array() } });
                        }
                        else if (id == "unchanged-scope") {
                            if (scopeEvents != 1 || response.at("options").at("sources") != json::array()) {
                                finish(false);
                                return;
                            }
                            send(connection, "set_library_scope", "malformed-scope", json::array());
                        }
                        else if (id == "malformed-scope") {
                            if (response.at("options").value("success", true)) {
                                finish(false);
                                return;
                            }
                            send(
                                connection, "set_library_scope", "unsupported-filter", { { "future", json::array() } });
                        }
                        else if (id == "unsupported-filter" || id == "negative-source" || id == "oversized-selection") {
                            if (response.at("options").value("success", true)) {
                                finish(false);
                                return;
                            }
                            if (id == "unsupported-filter") {
                                send(connection, "set_library_scope", "negative-source", { { "sources", { -1 } } });
                            }
                            else if (id == "negative-source") {
                                send(connection, "set_library_scope", "oversized-selection",
                                    { { "sources", std::vector<int>(MC_LIBRARY_SCOPE_MAX_SOURCES + 1, 0) } });
                            }
                            else {
                                send(connection, "query_tracks", "empty-tracks", json::object());
                            }
                        }
                        else if (id == "empty-tracks" || id == "empty-category" || id == "empty-albums" ||
                            id == "empty-category-tracks")
                        {
                            if (response.at("options").at("data") != json::array()) {
                                finish(false);
                                return;
                            }
                            if (id == "empty-tracks") {
                                send(connection, "query_category", "empty-category", { { "category", "artist" } });
                            }
                            else if (id == "empty-category") {
                                send(connection, "query_albums", "empty-albums", json::object());
                            }
                            else if (id == "empty-albums") {
                                send(connection, "query_tracks_by_category", "empty-category-tracks",
                                    { { "category", "artist" } });
                            }
                            else {
                                send(connection, "send_raw_query", "empty-raw",
                                    {
                                        { "raw_query_data",
                                            R"({"name":"SearchTrackListQuery","options":{"filter":"","sortType":0}})" },
                                    });
                            }
                        }
                        else if (id == "empty-raw") {
                            const auto raw =
                                json::parse(response.at("options").at("raw_query_data").template get<std::string>());
                            if (!raw.at("result").at("trackList").empty()) {
                                finish(false);
                                return;
                            }
                            if (scopeEvents != 1) {
                                finish(false);
                                return;
                            }
                            send(connection, "send_raw_query", "invalid-query-scope",
                                {
                                    { "raw_query_data",
                                        R"({"name":"SearchTrackListQuery","options":{"filter":"","sortType":0,"scope":{"future":[]}}})" },
                                });
                        }
                        else if (id == "invalid-query-scope") {
                            if (response.at("options").value("success", true)) {
                                finish(false);
                                return;
                            }
                            send(connection, "send_raw_query", "scope-sources",
                                {
                                    { "raw_query_data", R"({"name":"SourceListQuery","options":{}})" },
                                });
                        }
                        else if (id == "scope-sources") {
                            const auto raw =
                                json::parse(response.at("options").at("raw_query_data").template get<std::string>());
                            bool files = false;
                            for (const auto& source : raw.at("result")) {
                                files |= source.at("id") == 0 && source.at("label") == "Files";
                            }
                            if (!files) {
                                finish(false);
                                return;
                            }
                            send(connection, "query_tracks_by_category", "scoped-playlist-page",
                                {
                                    { "category", "playlists" },
                                    { "id", playlistId },
                                    { "limit", 1 },
                                    { "offset", 1 },
                                });
                        }
                        else if (id == "scoped-playlist-page") {
                            const auto& options = response.at("options");
                            if (options.at("data").size() != 1 || options.at("data")[0].at("id") != expectedId ||
                                options.at("limit") != 1 || options.at("offset") != 1)
                            {
                                finish(false);
                                return;
                            }
                            connectPeer();
                        }
                        else if (id == "peer-auth" || id == "reconnect-auth") {
                            if (!response.at("options").value("authenticated", false)) {
                                finish(false);
                                return;
                            }
                            send(connection, "get_library_scope", id == "peer-auth" ? "peer-scope" : "reconnect-scope",
                                json::object());
                        }
                        else if (id == "peer-scope" || id == "reconnect-scope") {
                            if (!response.at("options").at("sources").is_null()) {
                                finish(false);
                                return;
                            }
                            send(connection, "query_tracks", id == "peer-scope" ? "peer-tracks" : "reconnect-tracks",
                                json::object());
                        }
                        else if (id == "peer-tracks") {
                            if (response.at("options").at("data").empty()) {
                                finish(false);
                                return;
                            }
                            send(scopedConnection, "get_library_scope", "original-scope", json::object());
                        }
                        else if (id == "original-scope") {
                            if (response.at("options").at("sources") != json::array()) {
                                finish(false);
                                return;
                            }
                            send(connection, "clear_library_scope", "clear-scope", json::object());
                        }
                        else if (id == "clear-scope") {
                            send(connection, "query_tracks", "restored-tracks", json::object());
                        }
                        else if (id == "restored-tracks") {
                            if (response.at("options").at("data").empty()) {
                                finish(false);
                                return;
                            }
                            send(connection, "set_library_scope", "before-reconnect", { { "sources", json::array() } });
                        }
                        else if (id == "before-reconnect") {
                            if (scopeEvents != 3) {
                                finish(false);
                                return;
                            }
                            reconnecting = true;
                            client.close(connection, websocketpp::close::status::normal, "scope reconnect test");
                        }
                        else if (id == "reconnect-tracks") {
                            if (response.at("options").at("data").empty()) {
                                finish(false);
                                return;
                            }
                            send(connection, "send_raw_query", "unknown-query", {
                                { "raw_query_data", R"({"name":"UnknownQuery","options":{}})" },
                            });
                        }
                        else if (id == "unknown-query") {
                            if (response.at("options").value("success", true)) {
                                finish(false);
                                return;
                            }
                            send(connection, "send_raw_query", "raw-sql", {
                                { "raw_query_data", "SELECT * FROM tracks" },
                            });
                        }
                        else if (id == "raw-sql") {
                            finish(!response.at("options").value("success", true));
                        }
                    }
                    catch (...) {
                        finish(false);
                    }
                });
                websocketpp::lib::error_code error;
                auto connection = client.get_connection("ws://127.0.0.1:" + std::to_string(port), error);
                if (error) {
                    finish(false);
                }
                else {
                    client.connect(connection);
                    client.run();
                }
                if (!finished.load()) {
                    finish(false);
                }
            }
            catch (...) {
                succeeded.store(false);
                finished.store(true);
                Host::Wake(&host);
            }
        });
        const bool completed = waitFor(context, host, [&] { return finished.load(); });
        probe.join();
        if (!completed || !succeeded.load()) {
            std::fputs("Self-remote queue, category, or invalid-query check failed\n", stderr);
            return false;
        }
        saved.reset();
        DeletePlaylist(context, *library, playlistId, [&](mc_result status, int64_t, std::string_view) {
            saved = status;
        });
        return awaitSaved();
    }

    bool checkRemoteAudio(Context& context, Host& host, Playback& playback, std::string_view format, bool rangeFallback) {
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

        if (format == "mp3" && !checkEncodedResponse(remoteTrack.Id(0), rangeFallback)) {
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

        if (format.empty()) {
            if (!checkSelfRemoteQueue(context, host, playback, remoteTrack)) {
                return false;
            }
            /* switching between remote tracks must not block the server that
            supplies their metadata while playback notifications are pending. */
            TrackResult nextTrack;
            std::optional<mc_result> nextOpened;
            OpenTrackSearch(context, TrackSearch { "Evening Flight" },
                [&](mc_result status, TrackResult result, std::string_view) {
                    nextOpened = status;
                    nextTrack = std::move(result);
                });
            if (!waitFor(context, host, [&] { return nextOpened.has_value(); }) ||
                nextOpened != MC_OK || nextTrack.Size() != 1 || playback.Play(nextTrack, 0) != MC_OK ||
                !waitFor(context, host, [&] {
                    const auto current = playback.CurrentTrackMetadata();
                    return current && current->Id() == nextTrack.Id(0);
                }))
            {
                std::fputs("Could not switch between tracks from the local server\n", stderr);
                return false;
            }

            /* exercise the real server's category and playlist wire operations. */
            std::optional<bool> fieldsOk;
            ListCategoryFields(context, [&](mc_result status, std::vector<std::string> fields, std::string_view) {
                fieldsOk = status == MC_OK && std::ranges::find(fields, "artist") != fields.end();
            });
            if (!waitFor(context, host, [&] { return fieldsOk.has_value(); }) || !*fieldsOk) {
                std::fputs("Remote category fields did not round trip\n", stderr);
                return false;
            }
            auto library = context.GetLibrary();
            if (!library) {
                return false;
            }
            std::optional<mc_result> saved;
            int64_t playlistId = 0;
            SavePlaylist(context, *library, 0, "Server cutover fixture", { remoteTrack.Id(0) },
                [&](mc_result status, int64_t id, std::string_view) { saved = status; playlistId = id; });
            if (!waitFor(context, host, [&] { return saved.has_value(); }) || saved != MC_OK || playlistId <= 0) {
                std::fputs("Remote playlist creation failed\n", stderr);
                return false;
            }
            std::optional<bool> readOk;
            OpenPlaylist(context, *library, playlistId,
                [&](mc_result status, TrackResult tracks, std::string_view) {
                    readOk = status == MC_OK && tracks.Size() == 1 && tracks.Id(0) == remoteTrack.Id(0);
                });
            if (!waitFor(context, host, [&] { return readOk.has_value(); }) || !*readOk) {
                std::fputs("Remote playlist read failed\n", stderr);
                return false;
            }
            std::optional<mc_result> deleted;
            DeletePlaylist(context, *library, playlistId,
                [&](mc_result status, int64_t, std::string_view) { deleted = status; });
            if (!waitFor(context, host, [&] { return deleted.has_value(); }) || deleted != MC_OK) {
                std::fputs("Remote playlist deletion failed\n", stderr);
                return false;
            }
        }
        return true;
    }

    bool checkPluginSettings() {
        static constexpr char nullPlugin[] = "Null Output";
        static constexpr char sampleRate[] = "default_sample_rate";
        constexpr int32_t configuredRate = 44100;

        const auto catalog = ListPlugins();
        const auto null = std::ranges::find_if(catalog, [](const auto& plugin) {
            return plugin.name == nullPlugin;
        });
        if (null == catalog.end() || !null->enabled ||
            SetPluginEnabled(null->filename, false) != MC_OK)
        {
            std::fprintf(stderr, "C null plugin absent/disabled or toggle failed; catalog size=%zu\n", catalog.size());
            for (const auto& plugin : catalog) {
                std::fprintf(stderr, "plugin: %s (%s) enabled=%d\n", plugin.name.c_str(), plugin.id.c_str(), plugin.enabled);
            }
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

        /* null output has no device-specific settings. listing/enabling it
        still works; configuration reports absence rather than a fake schema. */
        const auto schema = ReadPluginSettings(null->id);
        const auto written = WritePluginSetting(null->id, sampleRate, configuredRate);
        if (schema || written != MC_ERROR_UNSUPPORTED) {
            std::fprintf(stderr, "C null output schema: present=%d write status=%d\n", bool(schema), written);
            return false;
        }
        return true;
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
        const mc_context_options& options, Host& host, std::optional<Library>& retained, bool equalizer,
        bool remoteAudio, std::string_view format, bool rangeFallback, LogProbe& log)
    {
        TrackResult tracks;
        mc_result complete = MC_ERROR_BUSY;
        bool wrongThread = false;
        const auto owner = std::this_thread::get_id();

        const auto beforeStartup = host.Generation();
        Context context(options);
        /* C plugin metadata belongs to the selected runtime session; inspect
        it after startup rather than triggering old discovery before context creation. */
        if (!checkPluginSettings()) {
            std::fputs("Plugin metadata/configuration did not round trip through the public API\n", stderr);
            return false;
        }
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
        if (!retained || ReadOutputConfiguration().selectedDriver != "Null Output") {
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
        if (!inState(MC_PLAYBACK_STOPPED) || playback.Control(MC_PLAYBACK_PAUSE) != MC_OK ||
            playback.Control(MC_PLAYBACK_RESUME) != MC_OK || !inState(MC_PLAYBACK_STOPPED))
        {
            std::fputs("Explicit pause/resume changed stopped playback\n", stderr);
            return false;
        }
        if (playback.Play(tracks, 0) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PLAYING); }))
        {
            std::fputs("Managed playback did not start\n", stderr);
            return false;
        }
        constexpr mc_playback_command pauseCommand = MC_PLAYBACK_PAUSE;
        constexpr mc_playback_command resumeCommand = MC_PLAYBACK_RESUME;
        if (playback.Control(pauseCommand) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PAUSED); }) ||
            playback.Control(pauseCommand) != MC_OK || !inState(MC_PLAYBACK_PAUSED) ||
            playback.Control(resumeCommand) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PLAYING); }) ||
            playback.Control(resumeCommand) != MC_OK || !inState(MC_PLAYBACK_PLAYING) ||
            playback.Control(pauseCommand) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PAUSED); }) ||
            playback.Control(MC_PLAYBACK_RELOAD_OUTPUT) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PREPARED); }))
        {
            std::fputs("Managed playback did not apply explicit pause/resume and output reload\n", stderr);
            return false;
        }
        if (playback.Control(MC_PLAYBACK_PAUSE) != MC_OK || !inState(MC_PLAYBACK_PREPARED) ||
            playback.Control(MC_PLAYBACK_RESUME) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PLAYING); }) ||
            playback.Control(MC_PLAYBACK_PAUSE) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PAUSED); }) ||
            playback.Control(MC_PLAYBACK_RELOAD_OUTPUT) != MC_OK ||
            !waitFor(context, host, [&] { return inState(MC_PLAYBACK_PREPARED); }))
        {
            std::fputs("Explicit pause/resume did not preserve prepared playback semantics\n", stderr);
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
            if (!checkRemoteAudio(context, host, playback, format, rangeFallback)) {
                return false;
            }
            if (format.empty()) {
                /* a failed restart keeps the new settings and stops the old
                listener; callers can correct the port and restart it. */
                const auto original = ReadServerConfiguration();
                if (!original) {
                    std::fputs("Could not read server settings before reload failure check\n", stderr);
                    return false;
                }
                asio::io_context sockets;
                using Tcp = asio::ip::tcp;
                Tcp::acceptor occupied(sockets);
                occupied.open(Tcp::v4());
#ifdef _WIN32
                /* Windows permits another listener to reuse a bound port unless
                the first socket requests exclusive ownership before bind. */
                const BOOL exclusive = TRUE;
                if (setsockopt(occupied.native_handle(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                        reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0)
                {
                    std::fputs("Could not reserve the test port exclusively\n", stderr);
                    return false;
                }
#endif
                occupied.bind({ Tcp::v4(), 0 });
                occupied.listen();
                auto unavailable = *original;
                unavailable.audioPort = occupied.local_endpoint().port();
                const auto failedReload = ConfigureServer(unavailable);
                if (failedReload != MC_ERROR_PLUGIN) {
                    std::fprintf(stderr, "Expected failed server restart, got %d\n", failedReload);
                    return false;
                }
                const auto saved = ReadServerConfiguration();
                if (!saved || saved->audioPort != unavailable.audioPort) {
                    std::fputs("Failed server restart did not retain its settings\n", stderr);
                    return false;
                }
                Tcp::socket previous(sockets);
                asio::error_code error;
                previous.connect({ asio::ip::address_v4::loopback(), original->audioPort }, error);
                if (error != asio::error::connection_refused || ConfigureServer(*original) != MC_OK) {
                    std::fputs("Failed server restart did not stop or recover the listener\n", stderr);
                    return false;
                }
            }
            const auto plugins = ListPlugins();
            const auto server = std::find_if(plugins.begin(), plugins.end(), [](const auto& plugin) {
                return plugin.filename.starts_with("libserver.") || plugin.filename == "server.dll";
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

        /* applying a plugin's settings must remain on the runtime owner thread. */
        std::mutex gateMutex;
        std::condition_variable gate;
        bool started = false;
        std::atomic<bool> finished { false };
        std::atomic<mc_result> offOwner { MC_ERROR_INTERNAL };
        std::atomic<bool> badSettingsAccess { false };
        std::thread configure([&] {
            offOwner.store(ApplyPluginSettings(serverPluginId));
            {
                std::lock_guard lock(gateMutex);
                started = true;
                gate.notify_one();
            }
            while (!finished.load()) {
                ApplyPluginSettings(serverPluginId);
                const mc_string_view id { serverPluginId.data(), serverPluginId.size() };
                const auto status = mc_plugin_read_settings(id, [](void* data, const mc_plugin_setting*) {
                    static_cast<std::atomic<bool>*>(data)->store(true);
                }, &badSettingsAccess);
                if (status != MC_ERROR_WRONG_THREAD && status != MC_ERROR_UNSUPPORTED) {
                    badSettingsAccess.store(true);
                }
            }
        });
        {
            std::unique_lock lock(gateMutex);
            if (!gate.wait_for(lock, 5s, [&] { return started; })) {
                finished.store(true);
                lock.unlock();
                configure.join();
                return false;
            }
        }
        const auto shutdown = context.Shutdown();
        finished.store(true);
        configure.join();
        if (offOwner.load() != MC_ERROR_WRONG_THREAD || badSettingsAccess.load() || shutdown != MC_OK || playback.Control(MC_PLAYBACK_STOP) == MC_OK ||
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
    bool equalizer = false;
    bool server = false;
    bool remoteAudio = false;
    bool synchronous = false;
    bool rangeFallback = false;
    bool noTranscodeCache = false;
    bool playbackOnly = false;
    std::string_view transcodeFormat;
    for (int i = 1; i < argc; ++i) {
        const std::string_view option(argv[i]);
        equalizer |= option == "--check-equalizer";
        server |= option == "--check-server";
        remoteAudio |= option == "--check-remote-audio";
        synchronous |= option == "--synchronous-transcode";
        rangeFallback |= option == "--range-fallback";
        noTranscodeCache |= option == "--no-transcode-cache";
        playbackOnly |= option == "--playback-only";
        constexpr std::string_view transcodeOption = "--transcode=";
        if (option.starts_with(transcodeOption)) {
            transcodeFormat = option.substr(transcodeOption.size());
        }
    }
    /* configure the plugin through its public schema before starting playback. */
    {
        auto setup = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        setup.services = MC_SERVICE_PLUGINS;
        Context plugins(setup);
        if (!checkServerConfiguration(server)) {
            std::fputs("Server plugin settings did not round trip\n", stderr);
            return 1;
        }
        if (remoteAudio && !configureLoopbackAudio(transcodeFormat, synchronous, noTranscodeCache)) {
            std::fputs("Could not configure the loopback server\n", stderr);
            return 1;
        }
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
    if (!checkRuntime(options, host, oldLibrary, equalizer, remoteAudio, transcodeFormat, rangeFallback, log)) {
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
