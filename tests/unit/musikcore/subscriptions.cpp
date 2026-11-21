#include "../../support/check.h"

#include <musikcore/support/Subscriptions.h>

#include <chrono>
#include <future>
#include <thread>

namespace {
    using musik::core::Subscriptions;
    using namespace std::chrono_literals;

    bool changesDuringDelivery() {
        Subscriptions<int> events;
        Subscriptions<int>::Connection first, removed, added;
        int firstValue = 0;
        int removedValue = 0;
        int addedValue = 0;

        first = events.Subscribe([&](int value) {
            firstValue = value;
            first.Reset();
            removed.Reset();
            added = events.Subscribe([&](int later) { addedValue = later; });
        });
        removed = events.Subscribe([&](int value) { removedValue = value; });

        events.Publish(1);
        events.Publish(2);
        return TEST_CHECK(firstValue == 1 && removedValue == 0 && addedValue == 2);
    }

    bool waitsForRunningCallback() {
        Subscriptions<> events;
        std::promise<void> entered, release, disconnecting, disconnected;
        auto released = release.get_future();
        auto running = entered.get_future();
        auto finished = disconnected.get_future();
        auto connection = events.Subscribe([&] {
            entered.set_value();
            released.wait();
        });
        std::jthread publisher([&] { events.Publish(); });
        if (running.wait_for(2s) != std::future_status::ready) {
            release.set_value();
            return false;
        }
        std::jthread closer([&] {
            disconnecting.set_value();
            connection.Reset();
            disconnected.set_value();
        });

        const bool started = disconnecting.get_future().wait_for(2s) == std::future_status::ready;
        const bool waited = finished.wait_for(50ms) == std::future_status::timeout;
        release.set_value();
        const bool completed = finished.wait_for(2s) == std::future_status::ready;
        return TEST_CHECK(started && waited && completed);
    }
}

int main() {
    return changesDuringDelivery() && waitsForRunningCallback() ? 0 : 1;
}
