#pragma once

#include <musikcore/debug.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/runtime/MessageQueue.h>

/* Neither copyable nor movable. Owns production services in application order.
   Dependent playback/library objects must be declared after this fixture. */
class CoreRuntime final {
public:
    CoreRuntime() {
        musik::debug::Start();
        try {
            musik::core::plugin::Init();
            musik::core::LibraryFactory::Initialize(this->queue);
        }
        catch (...) {
            musik::core::LibraryFactory::Shutdown();
            musik::core::plugin::Shutdown();
            musik::debug::Shutdown();
            throw;
        }
    }
    ~CoreRuntime() {
        musik::core::LibraryFactory::Shutdown();
        musik::core::plugin::Shutdown();
        musik::debug::Shutdown();
    }
    CoreRuntime(const CoreRuntime&) = delete;
    CoreRuntime& operator=(const CoreRuntime&) = delete;
    CoreRuntime(CoreRuntime&&) = delete;
    CoreRuntime& operator=(CoreRuntime&&) = delete;
    musik::core::runtime::MessageQueue& Queue() noexcept { return this->queue; }
private:
    musik::core::runtime::MessageQueue queue;
};
