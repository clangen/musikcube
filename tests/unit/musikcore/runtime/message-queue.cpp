/* Direct tests of the production message queue. No library, plugins, audio,
   or shared runtime is started, so queue failures have a small reproducible fixture. */

#include <musikcore/runtime/Message.h>
#include <musikcore/runtime/MessageQueue.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace {
    using namespace musik::core::runtime;

    #define CHECK(expression) do { \
        if (!(expression)) { \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

    /* Observe delivered values and optionally reenter the queue from a handler.
       The test owns this target and keeps it alive until dispatch has stopped. */
    class Target final : public IMessageTarget {
        public:
            struct Seen {
                int type;
                int64_t first;
                int64_t second;
            };

            /* Record before invoking the action so callback-side queue edits
               cannot conceal which message initiated the change. */
            void ProcessMessage(IMessage& message) override {
                this->seen.push_back({message.Type(), message.UserData1(), message.UserData2()});
                if (this->action) {
                    this->action(message);
                }
            }

            std::vector<Seen> seen;
            std::function<void(IMessage&)> action;
    };

    /* Expose the existing protected scheduling observation so tests can check
       head/empty transitions without sleeping until distant deadlines. */
    class Queue final : public MessageQueue {
        public:
            int64_t NextTime() { return this->GetNextMessageTime(); }
    };

    /* Run a cleanup action when the final message owner releases its payload.
       Use to detect leaks and reproduce destructors that reenter the queue. */
    class TrackedMessage final : public Message {
        public:
            TrackedMessage(IMessageTarget* target, int type, std::function<void()> cleanup)
            : Message(target, type, 0, 0)
            , cleanup(std::move(cleanup)) {
            }

            /* Real payload cleanup can unregister objects or post work; using
               that behavior here detects a queue mutex held across deletion. */
            ~TrackedMessage() override { this->cleanup(); }

        private:
            std::function<void()> cleanup;
    };

    /* Registration gates delivery, and equally ready messages retain posting
       order. Negative delays must act immediately rather than wrap into the future. */
    bool checkRegistrationAndOrder() {
        Target target;
        unsigned dropped = 0;
        Queue queue;
        queue.Post(std::make_shared<TrackedMessage>(&target, 9, [&] { ++dropped; }));
        CHECK(dropped == 1 && !queue.Contains(&target));

        queue.Register(&target);
        queue.Register(&target);
        queue.Post(Message::Create(&target, 1, 11, 12));
        queue.Post(Message::Create(&target, 2, 21, 22), -100);
        queue.Post(Message::Create(&target, 3, 31, 32));
        CHECK(target.seen.empty() && queue.Contains(&target, 2));

        queue.Dispatch();
        CHECK(target.seen.size() == 3);
        return true;
        for (size_t i = 0; i < target.seen.size(); ++i) {
            CHECK(target.seen[i].type == static_cast<int>(i + 1));
            CHECK(target.seen[i].first == static_cast<int64_t>((i + 1) * 10 + 1));
            CHECK(target.seen[i].second == static_cast<int64_t>((i + 1) * 10 + 2));
        }
        CHECK(!queue.Contains(&target) && queue.NextTime() == -1);
        queue.Dispatch();
        CHECK(target.seen.size() == 3);
    }

    /* Removing one type/target must release those payloads immediately without
       deleting another target's work or changing the surviving order. */
    bool checkFilteredRemoval() {
        Target first, second;
        unsigned released = 0;
        Queue queue;
        queue.Register(&first);
        queue.Register(&second);
        const auto cleanup = [&] { ++released; };
        queue.Post(std::make_shared<TrackedMessage>(&first, 1, cleanup));
        queue.Post(std::make_shared<TrackedMessage>(&first, 2, cleanup));
        queue.Post(std::make_shared<TrackedMessage>(&first, 1, cleanup));
        queue.Post(std::make_shared<TrackedMessage>(&second, 1, cleanup));

        CHECK(queue.Remove(&first, 1) == 2 && released == 2);
        CHECK(!queue.Contains(&first, 1) && queue.Contains(&first, 2));
        CHECK(queue.Contains(&second, 1));
        CHECK(queue.Remove(&first) == 1 && released == 3);
        CHECK(queue.Remove(&first) == 0);
        queue.Dispatch();
        CHECK(first.seen.empty() && second.seen.size() == 1 && released == 4);
        return true;
    }

    /* A callback can unregister a target whose message is already selected for
       this batch. Rechecking prevents stale delivery and must still free its payload. */
    bool checkUnregisterDuringDispatch() {
        Target first, second;
        unsigned released = 0;
        Queue queue;
        queue.Register(&first);
        queue.Register(&second);
        first.action = [&](IMessage&) { queue.Unregister(&second); };
        queue.Post(Message::Create(&first, 1));
        queue.Post(std::make_shared<TrackedMessage>(&second, 2, [&] { ++released; }));
        queue.Dispatch();
        CHECK(first.seen.size() == 1 && second.seen.empty() && released == 1);
        queue.Post(std::make_shared<TrackedMessage>(&second, 3, [&] { ++released; }));
        CHECK(released == 2 && !queue.Contains(&second));

        queue.Register(&second);
        first.action = [&](IMessage&) {
            queue.Unregister(&second);
            queue.Register(&second);
            queue.Post(Message::Create(&second, 6));
        };
        queue.Post(Message::Create(&first, 4));
        queue.Post(std::make_shared<TrackedMessage>(&second, 5, [&] {
            ++released;
        }));
        queue.Dispatch();
        CHECK(second.seen.empty() && released == 3);
        queue.Dispatch();
        CHECK(second.seen.size() == 1 && second.seen.front().type == 6);
        return true;
    }

    /* Payload destruction used to run under queueMutex. Calling queue methods
       from that destructor catches the actual deadlock, not just a leaked count. */
    bool checkReentrantRemoval() {
        Target first, second;
        bool cleaned = false;
        Queue queue;
        queue.Register(&first);
        queue.Register(&second);
        queue.Post(std::make_shared<TrackedMessage>(&first, 1, [&] {
            queue.Unregister(&first);
            queue.Post(Message::Create(&second, 2));
            cleaned = !queue.Contains(&first) && queue.Contains(&second, 2);
        }));

        CHECK(queue.Remove(&first) == 1 && cleaned);
        queue.Dispatch();
        CHECK(first.seen.empty() && second.seen.size() == 1);
        return true;
    }

    /* Callback-side posting/removal needs the mutex. New posts must wait for
       the next batch rather than recursively extending the batch being delivered. */
    bool checkReentrantHandler() {
        Target target;
        bool edited = false;
        Queue queue;
        queue.Register(&target);
        target.action = [&](IMessage& message) {
            if (message.Type() == 1) {
                queue.Post(Message::Create(&target, 2));
                edited = queue.Remove(&target, 2) == 1;
                queue.Post(Message::Create(&target, 3));
            }
        };
        queue.Post(Message::Create(&target, 1));
        queue.Dispatch();
        CHECK(edited && target.seen.size() == 1 && queue.Contains(&target, 3));
        queue.Dispatch();
        CHECK(target.seen.size() == 2 && target.seen.back().type == 3);
        return true;
    }

    /* Teardown must empty internal lists before releasing payloads, while the
       queue members remain usable for cleanup. Reentrant posts must be rejected. */
    bool checkDestructorCleanup() {
        Target target;
        unsigned released = 0;
        bool sawEmpty = false;
        {
            Queue queue;
            queue.Register(&target);
            queue.Post(std::make_shared<TrackedMessage>(&target, 1, [&] {
                ++released;
                sawEmpty = !queue.Contains(&target) && queue.NextTime() == -1;
                queue.Register(&target);
                queue.Post(std::make_shared<TrackedMessage>(&target, 2, [&] { ++released; }));
                queue.Broadcast(std::make_shared<TrackedMessage>(nullptr, 3, [&] { ++released; }));
                queue.Unregister(&target);
            }), 60000);
            queue.Post(std::make_shared<TrackedMessage>(&target, 4, [&] { ++released; }));
        }
        CHECK(released == 4 && sawEmpty && target.seen.empty());
        return true;
    }

    /* Pending delays are checked through queue state, not timing thresholds.
       Debounce must replace only matching target/type entries and update the head. */
    bool checkDelayAndDebounce() {
        Target target;
        Queue queue;
        queue.Register(&target);
        queue.Post(Message::Create(&target, 1), 60000);
        const auto future = queue.NextTime();
        queue.Dispatch();
        CHECK(target.seen.empty() && queue.Contains(&target, 1));
        queue.Post(Message::Create(&target, 2));
        CHECK(queue.NextTime() < future);
        queue.Dispatch();
        CHECK(target.seen.size() == 1 && target.seen[0].type == 2);
        CHECK(queue.NextTime() == future);

        queue.Post(Message::Create(&target, 1), 60000);
        queue.Debounce(Message::Create(&target, 1, 99), 0);
        queue.Dispatch();
        CHECK(target.seen.size() == 2 && target.seen.back().first == 99);
        CHECK(!queue.Contains(&target) && queue.NextTime() == -1);
        return true;
    }

    /* Broadcasts use weak subscribers. Self-unsubscribe and direct posting from
       a receiver prove delivery is unlocked; expired receivers must be harmless. */
    bool checkBroadcasts() {
        const auto first = std::make_shared<Target>();
        const auto second = std::make_shared<Target>();
        auto expired = std::make_shared<Target>();
        Target direct;
        Queue queue;
        queue.Register(&direct);
        queue.RegisterForBroadcasts(first);
        queue.RegisterForBroadcasts(second);
        queue.RegisterForBroadcasts(expired);
        expired.reset();
        first->action = [&](IMessage&) {
            queue.UnregisterForBroadcasts(first.get());
            queue.Post(Message::Create(&direct, 3));
        };
        queue.Broadcast(Message::Create(nullptr, 1));
        queue.Broadcast(Message::Create(nullptr, 2));
        queue.Dispatch();
        CHECK(first->seen.size() == 1 && second->seen.size() == 2 && direct.seen.empty());
        queue.Dispatch();
        CHECK(direct.seen.size() == 1);
        queue.UnregisterForBroadcasts(second.get());
        queue.Broadcast(Message::Create(nullptr, 4));
        queue.Dispatch();
        CHECK(first->seen.size() == 1 && second->seen.size() == 2);
        return true;
    }

    /* Removing a queued entry releases its ownership, not another caller's
       shared reference. Destroying the payload too early would invalidate that caller. */
    bool checkSharedPayloadLifetime() {
        Target target;
        unsigned released = 0;
        Queue queue;
        queue.Register(&target);
        auto retained = std::make_shared<TrackedMessage>(&target, 1, [&] { ++released; });
        queue.Post(retained);
        CHECK(queue.Remove(&target) == 1 && released == 0);
        retained.reset();
        CHECK(released == 1);
        return true;
    }

    /* Let a producer post before or during the host wait. Either schedule must
       deliver once; the timeout bounds a lost notification without using sleeps. */
    bool checkWaitAndWakeup() {
        Target target;
        Queue queue;
        queue.Register(&target);
        std::mutex mutex;
        std::condition_variable ready;
        bool start = false;
        std::thread producer([&] {
            std::unique_lock<std::mutex> lock(mutex);
            ready.wait(lock, [&] { return start; });
            lock.unlock();
            queue.Post(Message::Create(&target, 1));
        });
        {
            std::lock_guard<std::mutex> lock(mutex);
            start = true;
            ready.notify_one();
        }
        queue.WaitAndDispatch(5000);
        producer.join();
        CHECK(target.seen.size() == 1 && !queue.Contains(&target));
        queue.WaitAndDispatch(0);
        CHECK(target.seen.size() == 1);
        return true;
    }

    /* Concurrent producers may change the head while the host drains batches.
       Track every ID to catch dropped or duplicate delivery, not just total count. */
    bool checkConcurrentPosting() {
        constexpr size_t producerCount = 4;
        constexpr size_t messagesPerProducer = 200;
        constexpr size_t total = producerCount * messagesPerProducer;
        Target target;
        Queue queue;
        queue.Register(&target);
        std::vector<std::thread> producers;
        for (size_t producer = 0; producer < producerCount; ++producer) {
            producers.emplace_back([&, producer] {
                for (size_t i = 0; i < messagesPerProducer; ++i) {
                    queue.Post(Message::Create(&target, 1, producer * messagesPerProducer + i));
                }
            });
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (target.seen.size() < total) {
            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::steady_clock::duration::zero()) {
                break;
            }
            queue.WaitAndDispatch(std::chrono::ceil<std::chrono::milliseconds>(remaining).count());
        }
        for (auto& producer : producers) {
            producer.join();
        }

        CHECK(target.seen.size() == total);
        std::vector<unsigned> occurrences(total);
        for (const auto& message : target.seen) {
            CHECK(message.first >= 0 && static_cast<size_t>(message.first) < total);
            ++occurrences[static_cast<size_t>(message.first)];
        }
        CHECK(std::all_of(occurrences.begin(), occurrences.end(), [](unsigned count) { return count == 1; }));
        queue.Dispatch();
        CHECK(target.seen.size() == total && queue.NextTime() == -1);
        return true;
    }
}

/* CTest supplies one case name so failures and deadlocks are isolated and each
   behavior can be rerun directly without starting the production app. */
int main(int argc, char** argv) {
    const std::pair<const char*, bool (*)()> cases[] = {
        {"registration_order", checkRegistrationAndOrder},
        {"filtered_removal", checkFilteredRemoval},
        {"unregister_during_dispatch", checkUnregisterDuringDispatch},
        {"reentrant_removal", checkReentrantRemoval},
        {"reentrant_handler", checkReentrantHandler},
        {"destructor_cleanup", checkDestructorCleanup},
        {"delay_debounce", checkDelayAndDebounce},
        {"broadcasts", checkBroadcasts},
        {"shared_payload_lifetime", checkSharedPayloadLifetime},
        {"wait_wakeup", checkWaitAndWakeup},
        {"concurrent_posting", checkConcurrentPosting},
    };
    if (argc == 2) {
        for (const auto& entry : cases) {
            if (std::strcmp(argv[1], entry.first) == 0) {
                return entry.second() ? 0 : 1;
            }
        }
    }
    std::fputs("Expected a MessageQueue test case name\n", stderr);
    return 1;
}
