#include <musikcore/musikcore.hpp>
#include "host-wakeup.h"
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)>
        event(test_host_event_create(), test_host_event_destroy);
    if (!event) return 1;
    try {
        namespace api = musik::core::api;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();
        bool done = false;
        mc_result result = MC_ERROR_INTERNAL;
        api::Context context(options);
        api::Library library(argv[1]);
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
