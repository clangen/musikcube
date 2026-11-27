/* exercise URI playback through installed-style C/C++ surfaces with real
providers and a headless output. the parent supplies media and a loopback server. */
#include <musikcore/musikcore.hpp>
#include "../../support/host-wakeup.h"
#include "../../support/check.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <limits>

namespace {
    namespace api = musik::core::api;
    using Clock = std::chrono::steady_clock;

    /* wait through the real context wakeup/dispatch boundary with one deadline,
    so repeated unrelated events cannot hide a hung open or missed completion. */
    template<typename Ready>
    bool await(api::Context& context, test_host_event* event, Ready ready, const char* phase) {
        const auto deadline = Clock::now() + std::chrono::seconds(8);
        while (!ready()) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
            if (!TEST_CHECK(remaining.count() > 0) ||
                !TEST_CHECK(test_host_event_wait_for(event, (unsigned) remaining.count())) ||
                !TEST_CHECK(context.Dispatch() == MC_OK)) {
                std::cerr << "URI player timed out in " << phase << '\n';
                return false;
            }
        }
        return true;
    }

    /* sampling is separate from notifications, so tests assert observable state
    without depending on exactly how many intermediate changes coalesce. */
    mc_player_state state(mc_player* player) {
        mc_player_snapshot snapshot = MC_PLAYER_SNAPSHOT_DEFAULT;
        return mc_player_get_snapshot(player, &snapshot) == MC_OK ? snapshot.state : UINT32_MAX;
    }

    /* record delivery thread and reentrant destruction rejection. */
    struct Observer {
            std::thread::id owner = std::this_thread::get_id();
            mc_player** slot = nullptr;
            unsigned calls = 0;
            bool valid = true;

            /* callbacks may inspect state, but cannot destroy their active delivery. */
            static void MC_CALL Changed(void* data, const mc_player_snapshot*) {
                auto& self = *static_cast<Observer*>(data);
                ++self.calls;
                self.valid &= self.owner == std::this_thread::get_id();
                if (self.slot) {
                    self.valid &= mc_player_destroy(self.slot) == MC_ERROR_BUSY && *self.slot;
                }
            }
    };

    /* keep all C owners alive until context shutdown settles provider work,
    including assertion failures and retained handles after context destruction. */
    struct Players {
            mc_player* first = nullptr;
            mc_player* second = nullptr;
            ~Players() {
                mc_player_destroy(&this->second);
                mc_player_destroy(&this->first);
            }
    };

    /* prove the minimal service path never bootstraps a catalog as a side effect. */
    bool noCatalog() {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 std::filesystem::u8path(api::ReadPath(MC_PATH_DATA)))) {
            if (entry.path().filename() == "musik.db") {
                return false;
            }
        }
        return true;
    }

    /* use URI spelling that covers escaped spaces, non-ASCII, percent/hash, and
    filesystem-to-file URI conversion; the parent uses the same fixture path. */
    std::string fileUri(const std::string& path) {
        const auto bytes = std::filesystem::absolute(std::filesystem::u8path(path)).generic_u8string();
        std::string uri = "file://";
#ifdef WIN32
        uri += '/';
#endif
        const char hex[] = "0123456789ABCDEF";
        for (unsigned char c : bytes) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '/' || c == ':' || c == '-' || c == '.') {
                uri += (char) c;
            }
            else {
                uri += '%';
                uri += hex[c >> 4];
                uri += hex[c & 15];
            }
        }
        return uri;
    }

    /* A replaces B's callback after both messages have entered the same
    dispatch batch. B's old generation must not notify either observer; a later
    real change must still reach the replacement normally. */
    bool checkObserverReplacement(api::Context& context) {
        api::Player first(context), second(context);
        struct State {
                mc_player* second;
                unsigned oldCalls = 0;
                unsigned newCalls = 0;
        } state { second.Get() };
        second.SetCallback([](void* data, const mc_player_snapshot*) {
            ++static_cast<State*>(data)->oldCalls;
        },
            &state);
        first.SetCallback([](void* data, const mc_player_snapshot*) {
            auto& state = *static_cast<State*>(data);
            mc_player_set_callback(state.second, [](void* data, const mc_player_snapshot*) {
                ++static_cast<State*>(data)->newCalls;
            },
                &state);
        },
            &state);
        if (!TEST_CHECK(first.SetVolume(0.2) == MC_OK) || !TEST_CHECK(second.SetVolume(0.3) == MC_OK) ||
            !TEST_CHECK(context.Dispatch() == MC_OK) || !TEST_CHECK(state.oldCalls == 0 && state.newCalls == 0) ||
            !TEST_CHECK(second.SetVolume(0.4) == MC_OK) || !TEST_CHECK(context.Dispatch() == MC_OK)) {
            return false;
        }
        return TEST_CHECK(state.oldCalls == 0 && state.newCalls == 1);
    }

    /* replay a short chunked track first with disk caching disabled in the
    saved setting file, then with caching enabled through the public API.
    the loopback server counts requests to verify that the provider sees both. */
    bool checkHttpSettings(api::Context& context, test_host_event* event, mc_player* player, const std::string& http) {
        constexpr mc_string_view id { "b153adad-ee98-4331-ad32-4ff7f34828cd", 36 };
        struct Settings {
            size_t count = 0;
            int maxFiles = -1;
        } settings;

        if (!TEST_CHECK(mc_plugin_read_settings(id, [](void* data, const mc_plugin_setting* setting) {
            auto& observed = *static_cast<Settings*>(data);
            ++observed.count;
            if (std::string_view(setting->key.data, setting->key.length) == "max_cache_files") {
                observed.maxFiles = setting->value.data.integer;
            }
        }, &settings) == MC_OK) || !TEST_CHECK(settings.count == 5 && settings.maxFiles == 0)) {
            return false;
        }

        /* plugin schema access must reject worker threads before touching the registry. */
        mc_result readStatus = MC_OK;
        mc_result writeStatus = MC_OK;
        mc_result batchStatus = MC_OK;
        unsigned workerCallbacks = 0;
        std::thread worker([&] {
            const auto read = [](void* data, const mc_plugin_setting*) {
                ++*static_cast<unsigned*>(data);
            };
            mc_setting_entry entry {};
            entry.key = { "max_cache_files", 15 };
            entry.value.type = MC_SETTING_INTEGER;
            entry.value.data.integer = 1;
            readStatus = mc_plugin_read_settings(id, read, &workerCallbacks);
            writeStatus = mc_plugin_write_setting(id, &entry);
            batchStatus = mc_plugin_write_settings(id, &entry, 1);
        });
        worker.join();
        if (!TEST_CHECK(readStatus == MC_ERROR_WRONG_THREAD && writeStatus == MC_ERROR_WRONG_THREAD &&
            batchStatus == MC_ERROR_WRONG_THREAD && workerCallbacks == 0))
        {
            return false;
        }

        /* reject the entire batch when a later value violates the schema. */
        mc_setting_entry batch[2] {};
        batch[0].key = { "max_cache_files", 15 };
        batch[0].value.type = MC_SETTING_INTEGER;
        batch[0].value.data.integer = 4;
        batch[1].key = { "read_timeout_seconds", 20 };
        batch[1].value.type = MC_SETTING_INTEGER;
        batch[1].value.data.integer = 0;
        if (!TEST_CHECK(mc_plugin_write_settings(id, batch, 2) == MC_ERROR_INVALID_ARGUMENT)) {
            return false;
        }
        /* duplicate keys are ambiguous and must not change the saved value. */
        batch[1] = batch[0];
        batch[1].value.data.integer = 5;
        if (!TEST_CHECK(mc_plugin_write_settings(id, batch, 2) == MC_ERROR_INVALID_ARGUMENT)) {
            return false;
        }
        settings = {};
        if (!TEST_CHECK(mc_plugin_read_settings(id, [](void* data, const mc_plugin_setting* setting) {
            auto& observed = *static_cast<Settings*>(data);
            ++observed.count;
            if (std::string_view(setting->key.data, setting->key.length) == "max_cache_files") {
                observed.maxFiles = setting->value.data.integer;
            }
        }, &settings) == MC_OK) || !TEST_CHECK(settings.maxFiles == 0)) {
            return false;
        }

        const auto replay = [&](const char* route) {
            const auto url = http + route;
            for (unsigned pass = 0; pass < 2; ++pass) {
                if (!TEST_CHECK(mc_player_play_uri(player, { url.data(), url.size() }) == MC_OK) ||
                    !await(context, event, [&] { return state(player) == MC_PLAYER_FINISHED; }, route))
                {
                    return false;
                }
                if (pass == 0 && std::string_view(route) == "/cached") {
                    /* finished playback can precede the worker closing its source.
                    wait for that close to publish the cache before replaying it. */
                    const auto directory = std::filesystem::u8path(api::ReadPath(MC_PATH_DATA)) /
                        "cache" / "httpclient";
                    const auto prefix = "musikcube_" + std::to_string(std::hash<std::string> {}(url)) + "_";
                    const auto deadline = Clock::now() + std::chrono::seconds(3);
                    bool published = false;
                    while (!published && Clock::now() < deadline) {
                        std::error_code error;
                        for (std::filesystem::directory_iterator it(directory, error), end;
                            !error && it != end; it.increment(error))
                        {
                            if (it->path().filename().string().starts_with(prefix)) {
                                published = true;
                                break;
                            }
                        }
                        if (!published) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                    }
                    if (!TEST_CHECK(published)) {
                        return false;
                    }
                }
            }
            return true;
        };

        mc_setting_entry change {};
        change.key = { "read_timeout_seconds", 20 };
        change.value.type = MC_SETTING_INTEGER;
        change.value.data.integer = 0;
        if (!TEST_CHECK(mc_plugin_write_setting(id, &change) == MC_ERROR_INVALID_ARGUMENT) || !replay("/uncached")) {
            return false;
        }
        change.key = { "max_cache_files", 15 };
        change.value.data.integer = 4;
        return TEST_CHECK(mc_plugin_write_setting(id, &change) == MC_OK) && replay("/cached");
    }

    /* a cancelled download must discard only its own temporary file, even
    when another player has already completed and cached the same track. */
    bool checkOverlappingDownloads(api::Context& context, test_host_event* event, mc_player* first,
        const std::string& http)
    {
        Players peer;
        const auto url = http + "/overlap.wav";
        const mc_string_view uri { url.data(), url.size() };
        std::string ready;
        if (!TEST_CHECK(mc_player_play_uri(first, uri) == MC_OK) ||
            !TEST_CHECK(bool(std::getline(std::cin, ready)) && ready == "/overlap.wav") ||
            !TEST_CHECK(mc_player_create(context.Get(), &peer.first) == MC_OK) ||
            !TEST_CHECK(mc_player_play_uri(peer.first, uri) == MC_OK) ||
            !await(context, event, [&] { return state(peer.first) == MC_PLAYER_FINISHED; }, "concurrent HTTP EOF") ||
            !TEST_CHECK(mc_player_destroy(&peer.first) == MC_OK) || !TEST_CHECK(mc_player_stop(first) == MC_OK))
        {
            return false;
        }
        return TEST_CHECK(mc_player_play_uri(first, uri) == MC_OK) &&
            await(context, event, [&] { return state(first) == MC_PLAYER_FINISHED; }, "concurrent cache replay");
    }

    bool checkIndependentBlockedSession(api::Context& context, test_host_event* event,
        mc_player* stalled, const std::string& path, const std::string& http)
    {
        const auto blocked = http + "/blocked-independent.wav";
        std::string ready;
        if (!TEST_CHECK(mc_player_play_uri(stalled, { blocked.data(), blocked.size() }) == MC_OK) ||
            !TEST_CHECK(bool(std::getline(std::cin, ready)) && ready == "/blocked-independent.wav") ||
            !TEST_CHECK(state(stalled) == MC_PLAYER_BUFFERING)) {
            return false;
        }

        /* the server retains the first response body throughout the other
        session's playback and destruction. no timer releases the stalled read. */
        Players independent;
        if (!TEST_CHECK(mc_player_create(context.Get(), &independent.first) == MC_OK) ||
            !TEST_CHECK(mc_player_play_uri(independent.first, { path.data(), path.size() }) == MC_OK) ||
            !await(context, event, [&] { return state(independent.first) == MC_PLAYER_PLAYING; },
                "independent playing with stalled peer") ||
            !await(context, event, [&] { return state(independent.first) == MC_PLAYER_FINISHED; },
                "independent EOF with stalled peer") ||
            !TEST_CHECK(state(stalled) == MC_PLAYER_BUFFERING) ||
            !TEST_CHECK(mc_player_play_uri(independent.first, { path.data(), path.size() }) == MC_OK) ||
            !await(context, event, [&] { return state(independent.first) == MC_PLAYER_PLAYING; },
                "independent restart with stalled peer")) {
            return false;
        }

        const auto before = Clock::now();
        if (!TEST_CHECK(mc_player_destroy(&independent.first) == MC_OK) ||
            !TEST_CHECK(Clock::now() - before < std::chrono::seconds(3)) ||
            !TEST_CHECK(state(stalled) == MC_PLAYER_BUFFERING)) {
            return false;
        }
        return TEST_CHECK(mc_player_stop(stalled) == MC_OK);
    }

    bool checkInterruptedOpen(api::Context& context, test_host_event* event,
        mc_player* player, const std::string& path, const std::string& http)
    {
        /* Gate on a request whose response body the server never sends. */
        auto openBlocked = [&](const std::string& route) {
            const auto url = http + route;
            std::string ready;
            return TEST_CHECK(mc_player_play_uri(player, { url.data(), url.size() }) == MC_OK) &&
                TEST_CHECK(bool(std::getline(std::cin, ready)) && ready == route) &&
                TEST_CHECK(state(player) == MC_PLAYER_BUFFERING) &&
                TEST_CHECK(mc_player_seek(player, 0.1) == MC_ERROR_BUSY);
        };

        if (!openBlocked("/blocked-stop.wav")) {
            return false;
        }
        auto before = Clock::now();
        if (!TEST_CHECK(mc_player_stop(player) == MC_OK) ||
            !TEST_CHECK(Clock::now() - before < std::chrono::seconds(3)) ||
            !TEST_CHECK(context.Dispatch() == MC_OK) ||
            !TEST_CHECK(state(player) == MC_PLAYER_STOPPED) ||
            !openBlocked("/blocked-replace.wav")) {
            return false;
        }

        /* Reuse the same handle while the old decoder is still waiting for data. */
        before = Clock::now();
        return TEST_CHECK(mc_player_play_uri(player, { path.data(), path.size() }) == MC_OK) &&
            TEST_CHECK(Clock::now() - before < std::chrono::seconds(3)) &&
            await(context, event, [&] { return state(player) == MC_PLAYER_PLAYING; }, "replacement playing") &&
            await(context, event, [&] { return state(player) == MC_PLAYER_FINISHED; }, "replacement EOF");
    }

    /* control, independent lifetime, EOF, failure, and context-owned teardown
    all use the same production URI player rather than a mock transport. */
    bool check(const std::string& path, const std::string& http) {
        const std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> event(
            test_host_event_create(), test_host_event_destroy);
        if (!TEST_CHECK(bool(event))) {
            return false;
        }
        Observer observer;
        Players players;
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();
        api::Context context(options);
        if (!checkObserverReplacement(context)) {
            return false;
        }
        if (!TEST_CHECK(mc_player_create(context.Get(), &players.first) == MC_OK) ||
            !TEST_CHECK(context.Services() == MC_SERVICE_PLUGINS && !context.GetLibrary()) ||
            !TEST_CHECK(noCatalog())) {
            return false;
        }
        observer.slot = &players.first;
        if (!TEST_CHECK(mc_player_set_callback(players.first, Observer::Changed, &observer) == MC_OK)) {
            return false;
        }

        /* ownership and malformed inputs must reject without consuming live slots. */
        mc_player* original = players.first;
        bool wrongThread = false;
        std::thread other([&] {
            wrongThread = mc_player_stop(players.first) == MC_ERROR_WRONG_THREAD &&
                mc_player_destroy(&players.first) == MC_ERROR_WRONG_THREAD && players.first == original;
        });
        other.join();
        if (!TEST_CHECK(wrongThread) ||
            !TEST_CHECK(mc_player_create(context.Get(), &players.first) == MC_ERROR_INVALID_ARGUMENT) ||
            !TEST_CHECK(mc_player_play_uri(players.first, { nullptr, 1 }) == MC_ERROR_INVALID_ARGUMENT) ||
            !TEST_CHECK(mc_player_play_uri(players.first, { "file:///x%00y", 13 }) == MC_ERROR_INVALID_ARGUMENT) ||
            !TEST_CHECK(mc_player_play_uri(players.first, { "musikcore:private", 17 }) == MC_ERROR_UNSUPPORTED) ||
            !TEST_CHECK(mc_player_set_volume(players.first, std::numeric_limits<double>::quiet_NaN()) == MC_ERROR_INVALID_ARGUMENT) ||
            !TEST_CHECK(mc_player_seek(players.first, -1) == MC_ERROR_INVALID_ARGUMENT) ||
            !TEST_CHECK(mc_player_seek(players.first, 0) == MC_ERROR_NOT_FOUND)) {
            return false;
        }

        /* opening owns its URI bytes before returning, even during buffering. */
        std::string temporary = path;
        if (!TEST_CHECK(mc_player_play_uri(players.first, { temporary.data(), temporary.size() }) == MC_OK)) {
            return false;
        }
        temporary.assign(temporary.size(), 'x');
        if (!await(context, event.get(), [&] {
                return state(players.first) == MC_PLAYER_PLAYING;
            },
                "first playing") ||
            !TEST_CHECK(observer.valid && observer.calls > 0) || !TEST_CHECK(mc_player_set_volume(players.first, 0.25) == MC_OK) || !TEST_CHECK(mc_player_pause(players.first) == MC_OK) || !TEST_CHECK(state(players.first) == MC_PLAYER_PAUSED)) {
            return false;
        }

        /* closing another live player cannot wait on this paused player's worker. */
        if (!TEST_CHECK(mc_player_create(context.Get(), &players.second) == MC_OK) ||
            !TEST_CHECK(mc_player_play_uri(players.second, { path.data(), path.size() }) == MC_OK) ||
            !await(context, event.get(), [&] {
                return state(players.second) == MC_PLAYER_PLAYING;
            },
                "second playing")) {
            return false;
        }
        auto before = Clock::now();
        if (!TEST_CHECK(mc_player_destroy(&players.second) == MC_OK) ||
            !TEST_CHECK(Clock::now() - before < std::chrono::seconds(3)) ||
            !TEST_CHECK(state(players.first) == MC_PLAYER_PAUSED)) {
            return false;
        }

        /* a standalone seek resumes the selected track, including when the
        output is paused. verify EOF to catch a worker left waiting for Resume. */
        if (!TEST_CHECK(mc_player_seek(players.first, 0.1) == MC_OK) ||
            !await(context, event.get(), [&] {
                return state(players.first) == MC_PLAYER_FINISHED;
            },
                "seek EOF")) {
            return false;
        }
        auto snapshot = mc_player_snapshot MC_PLAYER_SNAPSHOT_DEFAULT;
        if (!TEST_CHECK(mc_player_get_snapshot(players.first, &snapshot) == MC_OK && snapshot.volume == 0.25) ||
            !TEST_CHECK(mc_player_seek(players.first, 0) == MC_ERROR_NOT_FOUND) ||
            !TEST_CHECK(state(players.first) == MC_PLAYER_FINISHED)) {
            return false;
        }

        /* file URI decoding and asynchronous failures must reach the same host loop. */
        const auto encoded = fileUri(path);
        if (!TEST_CHECK(mc_player_play_uri(players.first, { encoded.data(), encoded.size() }) == MC_OK) ||
            !await(context, event.get(), [&] {
                return state(players.first) == MC_PLAYER_FINISHED;
            },
                "file URI EOF")) {
            return false;
        }
        const std::string missing = path + ".missing";
        if (!TEST_CHECK(mc_player_play_uri(players.first, { missing.data(), missing.size() }) == MC_OK) ||
            !await(context, event.get(), [&] {
                return state(players.first) == MC_PLAYER_FAILED;
            },
                "missing source failure")) {
            return false;
        }

        /* cancellation before publication must never lose an interrupt or leave
        a queued callback targeting a destroyed URI player. */
        for (unsigned cycle = 0; cycle < 12; ++cycle) {
            if (!TEST_CHECK(mc_player_create(context.Get(), &players.second) == MC_OK) ||
                !TEST_CHECK(mc_player_play_uri(players.second, { path.data(), path.size() }) == MC_OK) ||
                !TEST_CHECK(mc_player_destroy(&players.second) == MC_OK)) {
                return false;
            }
        }
        if (!http.empty()) {
            if (!checkHttpSettings(context, event.get(), players.first, http) ||
                !checkOverlappingDownloads(context, event.get(), players.first, http) ||
                !checkIndependentBlockedSession(context, event.get(), players.first, path, http) ||
                !checkInterruptedOpen(context, event.get(), players.first, path, http)) {
                return false;
            }

            /* Exercise teardown after the server has rejected an open with 429. */
            const auto retry = http + "/retry.wav";
            std::string ready;
            if (!TEST_CHECK(mc_player_create(context.Get(), &players.second) == MC_OK) ||
                !TEST_CHECK(mc_player_play_uri(players.second, { retry.data(), retry.size() }) == MC_OK) ||
                !TEST_CHECK(bool(std::getline(std::cin, ready)) && ready == "/retry.wav")) {
                return false;
            }

            before = Clock::now();
            if (!TEST_CHECK(mc_player_destroy(&players.second) == MC_OK) ||
                !TEST_CHECK(Clock::now() - before < std::chrono::seconds(3))) {
                return false;
            }

            const auto url = http + "/tone.wav";
            if (!TEST_CHECK(mc_player_play_uri(players.first, { url.data(), url.size() }) == MC_OK) ||
                !await(context, event.get(), [&] {
                    return state(players.first) == MC_PLAYER_FINISHED;
                },
                    "HTTP EOF")) {
                return false;
            }
            /* a broken chunked response must fail and leave the next open free
            to download a complete copy at the same URL. */
            const auto truncated = http + "/truncated.wav";
            const mc_string_view interruptedUri { truncated.data(), truncated.size() };
            if (!TEST_CHECK(mc_player_play_uri(players.first, interruptedUri) == MC_OK) ||
                !await(context, event.get(), [&] { return state(players.first) == MC_PLAYER_FAILED; },
                    "truncated HTTP transfer") ||
                !TEST_CHECK(mc_player_play_uri(players.first, interruptedUri) == MC_OK) ||
                !await(context, event.get(), [&] { return state(players.first) == MC_PLAYER_FINISHED; },
                    "retry truncated transfer"))
            {
                return false;
            }

            /* a completed empty source is EOF, not an endless buffering wait. */
            const auto empty = http + "/empty.wav";
            if (!TEST_CHECK(mc_player_play_uri(players.first, { empty.data(), empty.size() }) == MC_OK) ||
                !await(context, event.get(), [&] { return state(players.first) == MC_PLAYER_FAILED; }, "empty HTTP failure"))
            {
                return false;
            }

            /* a response stays open after exceeding the default precache size.
            playback must start from readiness, without waiting for HTTP EOF. */
            const auto live = http + "/live.wav";
            if (!TEST_CHECK(mc_player_play_uri(players.first, { live.data(), live.size() }) == MC_OK) ||
                !await(context, event.get(), [&] {
                    return state(players.first) == MC_PLAYER_PLAYING;
                },
                    "live HTTP readiness") ||
                !TEST_CHECK(mc_player_stop(players.first) == MC_OK)) {
                return false;
            }
            const auto blocked = http + "/blocked.wav";
            if (!TEST_CHECK(mc_player_play_uri(players.first, { blocked.data(), blocked.size() }) == MC_OK)) {
                return false;
            }
            /* the parent signals only after the blocked request reaches its
            loopback server; closing must interrupt the decoder's blocked read. */
            if (!TEST_CHECK(bool(std::getline(std::cin, ready)) && ready == "/blocked.wav")) {
                return false;
            }
            before = Clock::now();
            if (!TEST_CHECK(mc_player_destroy(&players.first) == MC_OK) ||
                !TEST_CHECK(Clock::now() - before < std::chrono::seconds(3)) ||
                !TEST_CHECK(mc_player_create(context.Get(), &players.first) == MC_OK)) {
                return false;
            }
        }

        /* the C++ wrapper transfers exactly one owner using public headers only. */
        {
            api::Player first(context);
            api::Player moved(std::move(first));
            if (!TEST_CHECK(!first.Get() && moved.Get())) {
                return false;
            }
            first = std::move(moved);
            if (!TEST_CHECK(first.Get() && !moved.Get())) {
                return false;
            }
        }
        /* shutting down the context also interrupts live child work, not just
        idle handles. the HTTP parent acknowledges the second blocked request. */
        const auto closingUri = http.empty() ? path : http + "/blocked-context.wav";
        if (!TEST_CHECK(mc_player_set_callback(players.first, Observer::Changed, &observer) == MC_OK) ||
            !TEST_CHECK(mc_player_play_uri(players.first, { closingUri.data(), closingUri.size() }) == MC_OK)) {
            return false;
        }
        if (!http.empty()) {
            std::string ready;
            if (!TEST_CHECK(bool(std::getline(std::cin, ready)) && ready == "/blocked-context.wav")) {
                return false;
            }
        }
        else if (!await(context, event.get(), [&] {
                     return state(players.first) == MC_PLAYER_PLAYING;
                 },
                     "shutdown playing")) {
            return false;
        }
        const auto calls = observer.calls;
        before = Clock::now();
        if (!TEST_CHECK(context.Shutdown() == MC_OK) || !TEST_CHECK(context.Dispatch() == MC_OK) ||
            !TEST_CHECK(Clock::now() - before < std::chrono::seconds(3)) ||
            !TEST_CHECK(observer.calls == calls) || !TEST_CHECK(mc_player_stop(players.first) == MC_ERROR_NOT_FOUND) ||
            !TEST_CHECK(noCatalog()) || !TEST_CHECK(context.Close() == MC_OK) ||
            !TEST_CHECK(mc_player_destroy(&players.first) == MC_OK)) {
            return false;
        }
        return true;
    }
}

/* argv comes from an isolated fixture with no catalog, user media, or devices. */
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        return 1;
    }

    const auto path = std::filesystem::path(argv[1]).u8string();
    const auto http = std::filesystem::path(argc > 2 ? argv[2] : L"").u8string();
    return check(std::string(path.begin(), path.end()), std::string(http.begin(), http.end())) ? 0 : 1;
}
#else
int main(int argc, char** argv) {
    return argc >= 2 && check(argv[1], argc > 2 ? argv[2] : "") ? 0 : 1;
}
#endif
