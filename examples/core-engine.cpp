/* the header-only wrapper owns C handles; service startup and shutdown still
cross the public C ABI. no private core headers or C++ binary interface needed. */
#include <musikcore/musikcore.hpp>
#include "host-wakeup.h"
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)>
        event(test_host_event_create(), test_host_event_destroy);
    if (!event) {
        return 1;
    }
    try {
        namespace api = musik::core::api;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        const std::string_view plugins(argv[1]);
        options.plugin_directory = { plugins.data(), plugins.size() };
        options.flags = MC_CONTEXT_START_LOCAL;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();

        /* all services are the default. to omit scanning, request only Playback;
        Library and Plugins are added automatically and fixed until destruction. */
        bool done = false;
        mc_result result = MC_ERROR_INTERNAL;
        api::Context context(options);
        auto library = context.GetLibrary();
        auto playback = context.GetPlayback();
        if (!library || context.Services() != MC_SERVICE_ALL) {
            return 1;
        }
        api::QueryTracks(context, *library, {}, [&](mc_result status, api::TrackPage page, std::string_view error) {
            done = true;
            result = status;
            if (status != MC_OK) {
                std::cerr << error << '\n';
            }
            for (size_t row = 0; row < page.Size(); ++row) {
                std::cout << page.Track(row).Title() << '\n';
            }
        });
        while (!done && test_host_event_wait(event.get())) {
            if (context.Dispatch() != MC_OK) {
                break;
            }
        }

        /* stop before captured state leaves scope. retained library handles keep
        their origin; retained playback handles become inactive at shutdown. */
        const auto stopped = context.Shutdown();
        return done && result == MC_OK && stopped == MC_OK ? 0 : 1;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
