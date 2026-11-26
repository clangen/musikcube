/* explicitly select MC_SERVICE_NONE when supplying an existing read-only
catalog or remote session. the context still owns asynchronous requests,
cancellation, wakeups, and callback dispatch; it starts no engine services.
use core-engine.c/cpp instead to let the context own the configured engine. */
#include <musikcore/musikcore.hpp>
#include "host-wakeup.h"
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    /* optional source selection: a nonnegative catalog ID, or none for empty. */
    if (argc < 2 || argc > 3) {
        return 1;
    }
    std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)>
        event(test_host_event_create(), test_host_event_destroy);
    if (!event) return 1;
    try {
        namespace api = musik::core::api;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_NONE;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();
        bool done = false;
        mc_result result = MC_ERROR_INTERNAL;
        api::Context context(options);
        api::Library library(argv[1]);
        if (argc == 3) {
            const std::string selection(argv[2]);
            if (selection == "none") {
                library.SelectSources({});
            }
            else {
                size_t consumed = 0;
                const auto id = std::stoll(selection, &consumed);
                if (consumed != selection.size() || id < 0 || id > INT32_MAX) {
                    return 1;
                }
                library.SelectSources({ static_cast<int32_t>(id) });
            }
        }
        api::QueryTracks(context, library, {}, [&](mc_result status, api::TrackPage page, std::string_view error) {
            done = true;
            result = status;
            if (status != MC_OK) std::cerr << error << '\n';
            for (size_t row = 0; row < page.Size(); ++row) std::cout << page.Track(row).Title() << '\n';
        });
        while (!done && test_host_event_wait(event.get())) {
            if (context.Dispatch() != MC_OK) break;
        }
        /* Drain before callback captures go out of scope, including on timeout. */
        const bool passed = done && result == MC_OK;
        context.Shutdown();
        return passed ? 0 : 1;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
