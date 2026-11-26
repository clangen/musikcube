#pragma once

#include <musikcore/debug.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/LibraryBackends.h>
#include <musikcore/library.hpp>
#include <musikcore/playback.hpp>
#include <musikcore/plugin/Plugins.h>
#include <common/runtime/MessageQueue.h>

/* Neither copyable nor movable. Owns production services in application order.
   Dependent playback/library objects must be declared after this fixture. */
class CoreRuntime final {
public:
    /* Start debug, plugin, and library services in application order, rolling back partial initialization. */
    CoreRuntime() {
        musik::debug::Start();
        try {
            musik::core::plugin::Initialize({});
            musik::core::library::Initialize(this->queue);
        }
        catch (...) {
            musik::core::library::Shutdown();
            musik::core::plugin::UnbindServices();
            musik::core::plugin::Shutdown();
            musik::debug::Shutdown();
            throw;
        }
    }
    /* Stop library workers before unloading plugins and shutting down debugging. */
    ~CoreRuntime() {
        musik::core::library::Shutdown();
        musik::core::plugin::UnbindServices();
        musik::core::plugin::Shutdown();
        musik::debug::Shutdown();
    }
    CoreRuntime(const CoreRuntime&) = delete;
    CoreRuntime& operator=(const CoreRuntime&) = delete;
    CoreRuntime(CoreRuntime&&) = delete;
    CoreRuntime& operator=(CoreRuntime&&) = delete;
    /* Return the fixture-owned host queue; callers must not retain it beyond the fixture's lifetime. */
    musik::core::runtime::MessageQueue& Queue() noexcept { return this->queue; }
private:
    musik::core::runtime::MessageQueue queue;
};

std::shared_ptr<const musik::core::api::Library> OpenFixtureLibrary(
    musik::core::LibraryBackend::Type kind = musik::core::LibraryBackend::Type::Local);

inline musik::core::api::Playback OpenFixturePlayback(musik::core::audio::PlaybackService& service) {
    mc_playback* handle = nullptr;
    const auto result = service.OpenHandle(&handle);
    if (result != MC_OK) {
        throw musik::core::api::Error(result);
    }
    return musik::core::api::Playback(handle);
}
