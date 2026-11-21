#include "runtime-hardening-cases.h"
#include <musikcore/musikcore.hpp>
#include "../../support/check.h"
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>

#include <thread>

namespace runtime_tests {
    using namespace musik::core;

    /* exercise every selection in one process: omitted prerequisites must be
    added, disabled services must stay absent, and teardown must permit a
    different selection next time without retaining scanner/player state. */
    bool checkServices() {
        for (mc_services requested = MC_SERVICE_NONE; requested <= MC_SERVICE_ALL; ++requested) {
            auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
            options.services = requested;
            api::Context context(options);
            const auto effective = requested & (MC_SERVICE_PLAYBACK | MC_SERVICE_INDEXER)
                ? requested | MC_SERVICE_LIBRARY | MC_SERVICE_PLUGINS
                : requested;
            if (!TEST_CHECK(context.Services() == effective)) {
                return false;
            }

            /* a library remains queryable/configurable without a scanner. */
            const bool library = (effective & MC_SERVICE_LIBRARY) != 0;
            if (!TEST_CHECK(context.GetLibrary().has_value() == library) ||
                !TEST_CHECK(context.SelectLibrary(MC_LIBRARY_LOCAL) == (library ? MC_OK : MC_ERROR_UNSUPPORTED)))
            {
                return false;
            }
            if (library && !TEST_CHECK(bool(LibraryFactory::Instance().DefaultLocalLibrary()->Indexer()) ==
                bool(effective & MC_SERVICE_INDEXER)))
            {
                return false;
            }

            /* unavailable service calls reject cleanly, including subscriptions. */
            mc_playback* playback = nullptr;
            const bool player = (effective & MC_SERVICE_PLAYBACK) != 0;
            mc_indexer_state state = MC_INDEXER_IDLE;
            const auto scanner = effective & MC_SERVICE_INDEXER ? MC_OK : MC_ERROR_UNSUPPORTED;
            if (!TEST_CHECK(mc_context_get_playback(context.Get(), &playback) == (player ? MC_OK : MC_ERROR_UNSUPPORTED)) ||
                !TEST_CHECK(mc_indexer_get_state(context.Get(), &state) == scanner) ||
                !TEST_CHECK(mc_indexer_set_callback(context.Get(), nullptr, nullptr) == scanner))
            {
                mc_playback_destroy(&playback);
                return false;
            }
            mc_playback_destroy(&playback);

            /* selections are fixed and safe to inspect from a non-owner thread;
            service operations remain restricted to the creating thread. */
            bool offThread = false;
            std::thread reader([&] {
                mc_services observed = MC_SERVICE_NONE;
                offThread = mc_context_get_services(context.Get(), &observed) == MC_OK && observed == effective;
                if (effective) {
                    offThread = offThread && mc_context_shutdown(context.Get()) == MC_ERROR_WRONG_THREAD;
                }
            });
            reader.join();
            if (!TEST_CHECK(offThread) || !TEST_CHECK(context.Dispatch() == MC_OK) ||
                !TEST_CHECK(context.Shutdown() == MC_OK) || !TEST_CHECK(context.Services() == effective) ||
                !TEST_CHECK(context.Close() == MC_OK))
            {
                return false;
            }
        }

        /* NULL and an older sized prefix select the full engine. poisoned tail
        bytes must never override defaults when outside the advertised size. */
        mc_context* context = nullptr;
        mc_services services = MC_SERVICE_NONE;
        if (!TEST_CHECK(mc_context_create(nullptr, &context, nullptr) == MC_OK) ||
            !TEST_CHECK(mc_context_get_services(context, &services) == MC_OK && services == MC_SERVICE_ALL) ||
            !TEST_CHECK(mc_context_destroy(&context) == MC_OK))
        {
            mc_context_destroy(&context);
            return false;
        }
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.struct_size = offsetof(mc_context_options, services);
        options.services = UINT32_MAX;
        api::Context prefix(options);
        if (!TEST_CHECK(prefix.Services() == MC_SERVICE_ALL) || !TEST_CHECK(prefix.Close() == MC_OK)) {
            return false;
        }

        /* invalid combinations must fail before reserving process services. */
        options.struct_size = sizeof(options);
        if (!TEST_CHECK(mc_context_create(&options, &context, nullptr) == MC_ERROR_UNSUPPORTED && !context)) {
            return false;
        }
        options.services = MC_SERVICE_PLUGINS;
        options.flags = MC_CONTEXT_START_LOCAL;
        if (!TEST_CHECK(mc_context_create(&options, &context, nullptr) == MC_ERROR_UNSUPPORTED && !context)) {
            return false;
        }
        options.services = MC_SERVICE_NONE;
        options.flags = MC_CONTEXT_ENABLE_LOGGING;
        if (!TEST_CHECK(mc_context_create(&options, &context, nullptr) == MC_ERROR_UNSUPPORTED && !context)) {
            return false;
        }
        options.flags = 0;
        api::Context recovered(options);
        return TEST_CHECK(recovered.Services() == MC_SERVICE_NONE);
    }

    /* Inspect the private registry because its numeric identity is outside the public API.
    A repeated name must keep its origin, while different catalogs need
    distinct persisted IDs even when the settings file starts empty. */
    bool checkRegistry() {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_ALL;
        api::Context context(options);
        auto& factory = LibraryFactory::Instance();
        const auto local = factory.DefaultLocalLibrary();
        const auto remote = factory.DefaultRemoteLibrary();
        return TEST_CHECK(local->Id() != remote->Id()) &&
            TEST_CHECK(factory.CreateLibrary(local->Name(), local->GetType()) == local) &&
            TEST_CHECK(factory.CreateLibrary(remote->Name(), remote->GetType()) == remote);
    }

    /* successful startup must finish restoring preferences before a caller
    changes playback; dispatch must not silently overwrite that new command. */
    bool checkStartup() {
        const auto settings = Preferences::ForComponent(prefs::components::Settings);
        const auto remote = static_cast<int>(ILibrary::Type::Remote);
        settings->SetInt(prefs::keys::LibraryType, remote);

        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_ALL;
        options.flags = MC_CONTEXT_START_LOCAL;
        api::Context context(options);
        if (!TEST_CHECK(context.GetLibraryStatus().kind == MC_LIBRARY_LOCAL) ||
            !TEST_CHECK(settings->GetInt(prefs::keys::LibraryType, -1) == remote))
        {
            return false;
        }
        auto playback = context.GetPlayback();
        if (!TEST_CHECK(playback.Control(MC_PLAYBACK_SET_VOLUME, 0.375) == MC_OK) ||
            !TEST_CHECK(context.Dispatch() == MC_OK))
        {
            return false;
        }
        const auto state = playback.Snapshot();
        return TEST_CHECK(state && state->volume == 0.375);
    }

    /* wrong-thread calls must leave the context and playback usable by their
    owner; distinguish this contract violation from transient reentrancy. */
    bool checkOwner() {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_ALL;
        api::Context context(options);
        auto playback = context.GetPlayback();
        bool correct = false;
        std::thread other([&] {
            mc_context* retained = context.Get();
            mc_playback* unexpected = nullptr;
            correct = mc_context_dispatch(context.Get()) == MC_ERROR_WRONG_THREAD &&
                mc_context_shutdown(context.Get()) == MC_ERROR_WRONG_THREAD &&
                mc_context_destroy(&retained) == MC_ERROR_WRONG_THREAD && retained == context.Get() &&
                mc_context_get_playback(context.Get(), &unexpected) == MC_ERROR_WRONG_THREAD && !unexpected &&
                playback.Control(MC_PLAYBACK_STOP) == MC_ERROR_WRONG_THREAD &&
                context.SelectLibrary(MC_LIBRARY_REMOTE) == MC_ERROR_WRONG_THREAD;
        });
        other.join();
        return TEST_CHECK(correct) && TEST_CHECK(context.Dispatch() == MC_OK) &&
            TEST_CHECK(playback.Control(MC_PLAYBACK_STOP) == MC_OK);
    }
}
