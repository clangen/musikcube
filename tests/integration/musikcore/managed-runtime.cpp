#include <musikcore/musikcore.hpp>

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

int main(int argc, char**) {
    Host host;
    auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    options.flags = MC_CONTEXT_MANAGED_RUNTIME;
    options.wakeup = Host::Wake;
    options.wakeup_user_data = &host;
    std::optional<Library> oldLibrary;
    if (!checkRuntime(options, host, oldLibrary, argc > 1)) {
        return 1;
    }

    /* A retained catalog pins its closed backend safely across runtime restart. */
    Context reopened(options);
    oldLibrary.reset();
    return reopened.GetLibrary().has_value() ? 0 : 1;
}
