#pragma once
#include <musikcore/context.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/runtime/MessageQueue.h>
#include <chrono>

/* Wake the real host queue for ABI completions, without polling or sleeping. */
inline mc_context_options TestHostOptions(musik::core::runtime::MessageQueue& queue) {
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.wakeup_user_data = &queue;
    options.wakeup = [](void* data) {
        auto& host = *static_cast<musik::core::runtime::MessageQueue*>(data);
        /* A null-target Post is discarded; Broadcast is the queue's wake-only path. */
        constexpr int testWakeupMessage = 0x7fffffff;
        host.Broadcast(musik::core::runtime::Message::Create(nullptr, testWakeupMessage, 0, 0));
    };
    return options;
}

/* Pump real host messages while waiting so remote completions can progress without arbitrary sleeps or
   deadlocking the dispatch thread. */
template<typename Predicate>
bool AwaitTestHost(musik::core::runtime::MessageQueue& queue, Predicate ready) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ready()) {
        const auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero()) return false;
        queue.WaitAndDispatch(std::chrono::ceil<std::chrono::milliseconds>(remaining).count());
    }
    return true;
}
