#include "../../support/check.h"

#include <musikcore/subscriptions.hpp>

#include <atomic>
#include <barrier>
#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>

namespace {
    using musik::core::api::Subscriptions;
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
        std::promise<void> entered, releaseFirst, releaseSecond, disconnecting, disconnected;
        auto firstReleased = releaseFirst.get_future();
        auto secondReleased = releaseSecond.get_future();
        auto bothRunning = entered.get_future();
        auto finished = disconnected.get_future();
        std::atomic<int> invocations { 0 };
        auto connection = events.Subscribe([&] {
            if (invocations.fetch_add(1) == 0) {
                firstReleased.wait();
            }
            else {
                entered.set_value();
                secondReleased.wait();
            }
        });
        std::jthread firstPublisher([&] { events.Publish(); });
        std::jthread secondPublisher([&] { events.Publish(); });
        if (bothRunning.wait_for(2s) != std::future_status::ready) {
            releaseFirst.set_value();
            releaseSecond.set_value();
            return false;
        }
        std::jthread closer([&] {
            disconnecting.set_value();
            connection.Reset();
            disconnected.set_value();
        });

        const bool started = disconnecting.get_future().wait_for(2s) == std::future_status::ready;
        const bool waited = finished.wait_for(50ms) == std::future_status::timeout;
        releaseFirst.set_value();
        const bool waitedForBoth = finished.wait_for(50ms) == std::future_status::timeout;
        releaseSecond.set_value();
        const bool completed = finished.wait_for(2s) == std::future_status::ready;
        return TEST_CHECK(started && waited && waitedForBoth && completed);
    }

    bool crossDisconnect() {
        Subscriptions<> first;
        Subscriptions<int> second;
        Subscriptions<>::Connection firstConnection;
        Subscriptions<int>::Connection secondConnection;
        std::barrier bothRunning(2);

        firstConnection = first.Subscribe([&] {
            bothRunning.arrive_and_wait();
            secondConnection.Reset();
        });
        secondConnection = second.Subscribe([&](int) {
            bothRunning.arrive_and_wait();
            firstConnection.Reset();
        });

        std::jthread firstPublisher([&] { first.Publish(); });
        std::jthread secondPublisher([&] { second.Publish(1); });
        firstPublisher.join();
        secondPublisher.join();
        first.Publish();
        second.Publish(2);
        return true;
    }

    bool releasesCallbackThatThrows() {
        Subscriptions<> events;
        auto connection = events.Subscribe([] { throw std::runtime_error("callback failed"); });
        bool propagated = false;

        try {
            events.Publish();
        }
        catch (const std::runtime_error&) {
            propagated = true;
        }

        connection.Reset();
        events.Publish();
        return TEST_CHECK(propagated);
    }
}

int main() {
    return changesDuringDelivery() && waitsForRunningCallback() && crossDisconnect() &&
        releasesCallbackThatThrows() ? 0 : 1;
}
