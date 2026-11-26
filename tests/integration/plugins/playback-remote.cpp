#include <musikcore/musikcore.hpp>
#include <musikcore/plugin/playback-remote.h>
#include "../../support/host-wakeup.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

/* a failed requirement should identify the lifecycle case that failed. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "playback remote lifecycle: %s\n", message);
        std::abort();
    }
}

/* a remote starts only with playback, then joins before the plugin unloads. */
int main(int argc, char** argv) {
    const auto root = std::filesystem::path(std::getenv("XDG_CONFIG_HOME") ? std::getenv("XDG_CONFIG_HOME") : std::getenv("APPDATA"));
    const auto path = (root / "playback-remote.log").string();
#ifdef _WIN32
    _putenv_s("MUSIKCORE_TEST_REMOTE_LOG", path.c_str());
#else
    setenv("MUSIKCORE_TEST_REMOTE_LOG", path.c_str(), 1);
#endif
    auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_PLUGINS;
#ifndef _WIN32
    if (argc == 2 && std::string_view(argv[1]) == "startup-failure") {
        /* a failed remote wakes its own worker while the healthy remote continues. */
        int gate[2];
        require(pipe(gate) == 0, "create startup rollback gate");
        const auto readFd = std::to_string(gate[0]);
        const auto writeFd = std::to_string(gate[1]);
        setenv("MUSIKCORE_TEST_REMOTE_STARTFAIL_READ_FD", readFd.c_str(), 1);
        setenv("MUSIKCORE_TEST_REMOTE_STARTFAIL_WAKE_FD", writeFd.c_str(), 1);
        options.services = MC_SERVICE_PLAYBACK;
        options.flags = MC_CONTEXT_START_LOCAL;
        std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> startupWakeup(
            test_host_event_create(), test_host_event_destroy);
        require(bool(startupWakeup), "create startup wakeup");
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = startupWakeup.get();
        musik::core::api::Context broken(options);
        bool healthyCommand = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!healthyCommand && std::chrono::steady_clock::now() < deadline) {
            require(broken.Dispatch() == MC_OK, "dispatch healthy remote after peer failure");
            std::ifstream input(path);
            const std::string events((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            healthyCommand = events.find("\ncommand\n") != std::string::npos;
            if (!healthyCommand) {
                test_host_event_wait_for(startupWakeup.get(), 100);
            }
        }
        require(healthyCommand, "healthy remote continues after peer failure");
        require(broken.Close() == MC_OK, "close after remote startup failure");
        std::ifstream input(path);
        const std::string events((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        const auto failedJoin = events.find("peer-join\n");
        const auto healthyStop = events.find("\nstop\n");
        require(events.find("peer-released\n") != std::string::npos && events.find("timeout\n") == std::string::npos &&
            failedJoin != std::string::npos && healthyStop != std::string::npos && failedJoin < healthyStop,
            "failed remote joins independently while the healthy remote remains active");
        close(gate[0]);
        close(gate[1]);
        return 0;
    }
#endif
    if (argc == 2 && std::string_view(argv[1]) == "two") {
        /* every remote must stop accepting work before any remote is joined. */
#ifndef _WIN32
        int gate[2];
        require(pipe(gate) == 0, "create cross-remote shutdown gate");
        const auto readFd = std::to_string(gate[0]);
        const auto writeFd = std::to_string(gate[1]);
        setenv("MUSIKCORE_TEST_REMOTE_READ_FD", readFd.c_str(), 1);
        setenv("MUSIKCORE_TEST_REMOTE_WRITE_FD", writeFd.c_str(), 1);
#endif
        options.services = MC_SERVICE_PLAYBACK;
        options.flags = MC_CONTEXT_START_LOCAL;
        musik::core::api::Context both(options);
        require(both.Close() == MC_OK, "close both remotes");
        std::ifstream input(path);
        const std::string events((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        const auto firstStop = events.find("\nstop\n");
        const auto peerStop = events.find("\npeer-stop\n");
        const auto firstJoin = events.find("\njoin\n");
        const auto peerJoin = events.find("\npeer-join\n");
        require(firstStop != std::string::npos && peerStop != std::string::npos &&
            firstJoin != std::string::npos && peerJoin != std::string::npos &&
            firstStop < peerJoin && peerStop < firstJoin, "stop all remotes before joining either");
#ifndef _WIN32
        require(events.find("\nreleased\n") != std::string::npos && events.find("timeout\n") == std::string::npos,
            "one remote's worker exits after the other remote stops");
        close(gate[0]);
        close(gate[1]);
#endif
        return 0;
    }
    if (argc == 2 && (std::string_view(argv[1]) == "throw" || std::string_view(argv[1]) == "throw-start")) {
        /* a foreign lifecycle exception must abort before uncertain cleanup. */
        const bool throwStart = std::string_view(argv[1]) == "throw-start";
#ifdef _WIN32
        _putenv_s(throwStart ? "MUSIKCORE_TEST_REMOTE_THROW_START" : "MUSIKCORE_TEST_REMOTE_THROW", "1");
#else
        setenv(throwStart ? "MUSIKCORE_TEST_REMOTE_THROW_START" : "MUSIKCORE_TEST_REMOTE_THROW", "1", 1);
#endif
        options.services = MC_SERVICE_PLAYBACK;
        options.flags = MC_CONTEXT_START_LOCAL;
        musik::core::api::Context broken(options);
        require(false, "foreign factory exception did not terminate the process");
    }
    {
        /* discovering plugins alone must not start a playback remote. */
        musik::core::api::Context pluginsOnly(options);
        require(pluginsOnly.Close() == MC_OK, "close plugins-only context");
        std::ifstream input(path);
        const std::string events((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        require(events == "unload\n", "remote stays inactive without playback");
    }
    std::ofstream(path, std::ios::trunc).close();

    std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> wakeup(
        test_host_event_create(), test_host_event_destroy);
    require(bool(wakeup), "create dispatcher wakeup");
    options.services = MC_SERVICE_PLAYBACK;
    options.flags = MC_CONTEXT_START_LOCAL;
    options.wakeup = test_host_event_signal;
    options.wakeup_user_data = wakeup.get();
    musik::core::api::Context context(options);

    /* trigger a real playback change and dispatch it to the loaded plugin. */
    mc_playback* playback = nullptr;
    require(mc_context_get_playback(context.Get(), &playback) == MC_OK, "open playback");
    require(mc_playback_queue_clear(playback) == MC_OK, "clear queue");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool delivered = false;
    while (!delivered && std::chrono::steady_clock::now() < deadline) {
        require(context.Dispatch() == MC_OK, "dispatch playback change");
        std::ifstream input(path);
        const std::string events((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        delivered = events.find("volume\n") != std::string::npos && events.find("queue\n") != std::string::npos &&
            events.find("command\n") != std::string::npos;
        if (!delivered) {
            test_host_event_wait_for(wakeup.get(), 100);
        }
    }
    require(delivered, "remote receives playback and queue changes");
    mc_playback_destroy(&playback);
    require(context.Close() == MC_OK, "close context");
    std::ifstream input(path);
    const std::string events((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    require(events.find("create\nstart\ninitial\n") == 0 &&
        events.find("stop\njoin\ndestroy\nunload\n") != std::string::npos &&
        events.find("wrong-thread\n") == std::string::npos, "remote shutdown order and callback thread");
    input.close();

    /* a failed start may have created workers and must still be stopped and joined. */
    std::ofstream(path, std::ios::trunc).close();
#ifdef _WIN32
    _putenv_s("MUSIKCORE_TEST_REMOTE_FAIL", "1");
#else
    setenv("MUSIKCORE_TEST_REMOTE_FAIL", "1", 1);
#endif
    musik::core::api::Context failedStart(options);
    require(failedStart.Close() == MC_OK, "close after failed remote start");
    std::ifstream failedInput(path);
    const std::string failedEvents((std::istreambuf_iterator<char>(failedInput)), std::istreambuf_iterator<char>());
    require(failedEvents == "create\nstart\nstop\njoin\ndestroy\nunload\n", "failed start cleanup order");
    failedInput.close();

    /* a queued command must complete as cancelled before its plugin is unloaded. */
    std::ofstream(path, std::ios::trunc).close();
#ifdef _WIN32
    _putenv_s("MUSIKCORE_TEST_REMOTE_FAIL", "");
    _putenv_s("MUSIKCORE_TEST_REMOTE_HOLD", "1");
#else
    unsetenv("MUSIKCORE_TEST_REMOTE_FAIL");
    setenv("MUSIKCORE_TEST_REMOTE_HOLD", "1", 1);
#endif
    musik::core::api::Context pending(options);
    require(pending.Close() == MC_OK, "close with queued command");
    std::ifstream cancelledInput(path);
    const std::string cancelledEvents((std::istreambuf_iterator<char>(cancelledInput)), std::istreambuf_iterator<char>());
    require(cancelledEvents == "create\nstart\ninitial\nstop\ncancelled\njoin\ndestroy\nunload\n",
        "queued command settles before remote unload");
    cancelledInput.close();

    /* failed start must also settle commands submitted by workers before joining them. */
    std::ofstream(path, std::ios::trunc).close();
#ifdef _WIN32
    _putenv_s("MUSIKCORE_TEST_REMOTE_FAIL_AFTER_SUBMIT", "1");
#else
    setenv("MUSIKCORE_TEST_REMOTE_FAIL_AFTER_SUBMIT", "1", 1);
#endif
    musik::core::api::Context failedPending(options);
    require(failedPending.Close() == MC_OK, "close after failed start with queued command");
    std::ifstream failedPendingInput(path);
    const std::string failedPendingEvents(
        (std::istreambuf_iterator<char>(failedPendingInput)), std::istreambuf_iterator<char>());
    require(failedPendingEvents == "create\nstart\ninitial\nstop\ncancelled\njoin\ndestroy\nunload\n",
        "failed start settles commands before joining");
    failedPendingInput.close();

    /* saturation rejects extra work, and shutdown settles every accepted command. */
    std::ofstream(path, std::ios::trunc).close();
#ifdef _WIN32
    _putenv_s("MUSIKCORE_TEST_REMOTE_HOLD", "");
    _putenv_s("MUSIKCORE_TEST_REMOTE_FAIL_AFTER_SUBMIT", "");
    _putenv_s("MUSIKCORE_TEST_REMOTE_FLOOD", "1");
#else
    unsetenv("MUSIKCORE_TEST_REMOTE_HOLD");
    unsetenv("MUSIKCORE_TEST_REMOTE_FAIL_AFTER_SUBMIT");
    setenv("MUSIKCORE_TEST_REMOTE_FLOOD", "1", 1);
#endif
    musik::core::api::Context full(options);
    require(full.Close() == MC_OK, "close with full command queue");
    std::ifstream fullInput(path);
    const std::string fullEvents((std::istreambuf_iterator<char>(fullInput)), std::istreambuf_iterator<char>());
    std::string expected = "create\nstart\ninitial\nfull\nstop\n";
    for (unsigned i = 0; i < MC_PLAYBACK_REMOTE_MAX_PENDING_COMMANDS; ++i) {
        expected += "cancelled\n";
    }
    expected += "join\ndestroy\nunload\n";
    require(fullEvents == expected, "full queue rejects extra command and drains accepted work");
    fullInput.close();

    /* race a real worker's submissions with teardown and check every accepted request settles. */
    std::ofstream(path, std::ios::trunc).close();
#ifdef _WIN32
    _putenv_s("MUSIKCORE_TEST_REMOTE_FLOOD", "");
    _putenv_s("MUSIKCORE_TEST_REMOTE_RACE", "1");
#else
    unsetenv("MUSIKCORE_TEST_REMOTE_FLOOD");
    setenv("MUSIKCORE_TEST_REMOTE_RACE", "1", 1);
#endif
    musik::core::api::Context race(options);
    require(race.Close() == MC_OK, "close while worker submits commands");
    std::ifstream raceInput(path);
    const std::string raceEvents((std::istreambuf_iterator<char>(raceInput)), std::istreambuf_iterator<char>());
    require(raceEvents == "create\nstart\ninitial\nstop\nsettled\njoin\ndestroy\nunload\n",
        "all requests accepted during shutdown complete before destruction");
    raceInput.close();

    /* dispatch commands while the worker is submitting, then close during activity. */
    std::ofstream(path, std::ios::trunc).close();
    musik::core::api::Context live(options);
    bool ranConcurrently = false;
    const auto liveDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ranConcurrently && std::chrono::steady_clock::now() < liveDeadline) {
        require(live.Dispatch() == MC_OK, "dispatch while worker submits commands");
        std::ifstream input(path);
        const std::string events((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        ranConcurrently = events.find("running\n") != std::string::npos;
        if (!ranConcurrently) {
            test_host_event_wait_for(wakeup.get(), 100);
        }
    }
    require(ranConcurrently, "dispatch overlaps worker submissions");
    require(live.Close() == MC_OK, "close after concurrent dispatch");
    std::ifstream liveInput(path);
    const std::string liveEvents((std::istreambuf_iterator<char>(liveInput)), std::istreambuf_iterator<char>());
    require(liveEvents.find("settled\njoin\ndestroy\nunload\n") != std::string::npos &&
        liveEvents.find("submit-error\n") == std::string::npos &&
        liveEvents.find("wrong-thread\n") == std::string::npos, "concurrent requests settle before unload");
    liveInput.close();

    /* a disconnected client does not revoke a command already accepted by the host. */
    std::ofstream(path, std::ios::trunc).close();
#ifdef _WIN32
    _putenv_s("MUSIKCORE_TEST_REMOTE_RACE", "");
    _putenv_s("MUSIKCORE_TEST_REMOTE_DISCONNECT", "1");
#else
    unsetenv("MUSIKCORE_TEST_REMOTE_RACE");
    setenv("MUSIKCORE_TEST_REMOTE_DISCONNECT", "1", 1);
#endif
    musik::core::api::Context disconnected(options);
    bool commandCompleted = false;
    const auto disconnectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!commandCompleted && std::chrono::steady_clock::now() < disconnectDeadline) {
        require(disconnected.Dispatch() == MC_OK, "dispatch command after disconnect");
        std::ifstream input(path);
        const std::string events((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        const auto disconnect = events.find("disconnected\n");
        const auto completion = events.find("command\n");
        commandCompleted = disconnect != std::string::npos && completion != std::string::npos && disconnect < completion;
        if (!commandCompleted) {
            test_host_event_wait_for(wakeup.get(), 100);
        }
    }
    require(commandCompleted, "accepted command completes after client disconnect");
    require(disconnected.Close() == MC_OK, "close after disconnected client");

    /* a remote can read local category fields on its network worker. */
    std::ofstream(path, std::ios::trunc).close();
#ifdef _WIN32
    _putenv_s("MUSIKCORE_TEST_REMOTE_DISCONNECT", "");
    _putenv_s("MUSIKCORE_TEST_REMOTE_LIBRARY", "1");
#else
    unsetenv("MUSIKCORE_TEST_REMOTE_DISCONNECT");
    setenv("MUSIKCORE_TEST_REMOTE_LIBRARY", "1", 1);
#endif
    musik::core::api::Context library(options);
    require(library.Close() == MC_OK, "close after library read");
    std::ifstream libraryInput(path);
    const std::string libraryEvents((std::istreambuf_iterator<char>(libraryInput)), std::istreambuf_iterator<char>());
    require(libraryEvents == "create\nstart\ninitial\nlibrary-fields\nstop\njoin\ndestroy\nunload\n",
        "remote reads library fields before shutdown");

    /* a queued playback handle request is cancelled before its plugin unloads. */
    std::ofstream(path, std::ios::trunc).close();
#ifdef _WIN32
    _putenv_s("MUSIKCORE_TEST_REMOTE_LIBRARY", "");
    _putenv_s("MUSIKCORE_TEST_REMOTE_ACCESS", "1");
#else
    unsetenv("MUSIKCORE_TEST_REMOTE_LIBRARY");
    setenv("MUSIKCORE_TEST_REMOTE_ACCESS", "1", 1);
#endif
    musik::core::api::Context pendingAccess(options);
    require(pendingAccess.Close() == MC_OK, "close with pending playback access");
    std::ifstream pendingAccessInput(path);
    const std::string pendingAccessEvents(
        (std::istreambuf_iterator<char>(pendingAccessInput)), std::istreambuf_iterator<char>());
    require(pendingAccessEvents == "create\nstart\ninitial\nstop\naccess-cancelled\njoin\ndestroy\nunload\n",
        "pending playback access is cancelled before unload");
    pendingAccessInput.close();

    /* a dispatched access can use the public playback API on the owner thread. */
    std::ofstream(path, std::ios::trunc).close();
    musik::core::api::Context activeAccess(options);
    bool accessCompleted = false;
    const auto accessDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!accessCompleted && std::chrono::steady_clock::now() < accessDeadline) {
        require(activeAccess.Dispatch() == MC_OK, "dispatch playback access");
        std::ifstream current(path);
        const std::string currentEvents((std::istreambuf_iterator<char>(current)), std::istreambuf_iterator<char>());
        accessCompleted = currentEvents.find("access-ok\n") != std::string::npos;
        if (!accessCompleted) {
            test_host_event_wait_for(wakeup.get(), 100);
        }
    }
    require(accessCompleted, "playback access runs on the owner thread");
    require(activeAccess.Close() == MC_OK, "close after playback access");

    /* the indexer service uses the same owner-thread command and shutdown
    contract, independently of the playback service table. */
#ifdef _WIN32
    _putenv_s("MUSIKCORE_TEST_REMOTE_ACCESS", "");
    _putenv_s("MUSIKCORE_TEST_REMOTE_INDEXER", "1");
#else
    unsetenv("MUSIKCORE_TEST_REMOTE_ACCESS");
    setenv("MUSIKCORE_TEST_REMOTE_INDEXER", "1", 1);
#endif
    options.services = MC_SERVICE_PLAYBACK | MC_SERVICE_INDEXER;
    const auto readEvents = [&] {
        std::ifstream input(path);
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    };
    std::ofstream(path, std::ios::trunc).close();
    musik::core::api::Context pendingIndexer(options);
    require(pendingIndexer.Close() == MC_OK, "close with queued indexer scheduling");
    const auto cancelledIndexer = readEvents();
    const auto cancelledAt = cancelledIndexer.find("cancelled\n");
    require(cancelledAt != std::string::npos &&
            cancelledIndexer.find("cancelled\n", cancelledAt + 1) == std::string::npos &&
            cancelledIndexer.find("stop\n") < cancelledAt && cancelledAt < cancelledIndexer.find("join\n") &&
            cancelledIndexer.find("wrong-thread") == std::string::npos,
        "queued indexer completion cancels once on owner before join");

    std::ofstream(path, std::ios::trunc).close();
    musik::core::api::Context activeIndexer(options);
    bool indexerCompleted = false;
    const auto indexerDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!indexerCompleted && std::chrono::steady_clock::now() < indexerDeadline) {
        require(activeIndexer.Dispatch() == MC_OK, "dispatch plugin indexer scheduling");
        indexerCompleted = readEvents().find("command\n") != std::string::npos;
        if (!indexerCompleted) {
            test_host_event_wait_for(wakeup.get(), 100);
        }
    }
    require(indexerCompleted && activeIndexer.Close() == MC_OK, "plugin indexer scheduling completes");
    const auto indexed = readEvents();
    const auto completedAt = indexed.find("command\n");
    require(indexed.find("command\n", completedAt + 1) == std::string::npos &&
            indexed.find("wrong-thread") == std::string::npos &&
            indexed.find("late-indexer-accepted") == std::string::npos && completedAt < indexed.find("stop\n"),
        "indexer scheduling completes once on owner before shutdown");
    return 0;
}
