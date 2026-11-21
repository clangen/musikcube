#pragma once

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace musik { namespace core {

    /* Owns a typed list of callbacks. Neither copyable nor movable. Delivery
       runs on the publisher thread. Concurrent publishes may overlap. */
    template<class... Args>
    class Subscriptions {
        using Callback = std::function<void(Args...)>;

        struct Slot {
            explicit Slot(Callback callback) : callback(std::move(callback)) {}

            const Callback callback;
            std::mutex mutex;
            std::condition_variable idle;
            unsigned running {};
            bool connected { true };
        };

        /* Tracks reentrant disconnect without holding any lock during a callback. */
        struct Invocation {
            explicit Invocation(Slot& slot) : slot(slot), previous(Current()) {
                Current() = this;
            }

            ~Invocation() {
                Current() = this->previous;
                std::lock_guard<std::mutex> lock(this->slot.mutex);
                if (--this->slot.running == 0) {
                    this->slot.idle.notify_all();
                }
            }

            static Invocation*& Current() {
                static thread_local Invocation* current = nullptr;
                return current;
            }

            Slot& slot;
            Invocation* previous;
        };

        using SlotList = std::vector<std::weak_ptr<Slot>>;

    public:
        /* Move-only owner. Reset waits for admitted callbacks, so call it
           outside locks those callbacks need. Self-disconnect skips the wait. */
        class Connection {
        public:
            Connection() = default;
            Connection(Connection&&) noexcept = default;
            Connection(const Connection&) = delete;

            ~Connection() { this->Reset(); }

            Connection& operator=(Connection&& other) noexcept {
                if (this != &other) {
                    this->Reset();
                    this->slot = std::move(other.slot);
                }
                return *this;
            }
            Connection& operator=(const Connection&) = delete;

            void Reset() {
                auto slot = std::move(this->slot);
                if (!slot) {
                    return;
                }

                std::unique_lock<std::mutex> lock(slot->mutex);
                slot->connected = false;
                for (auto* call = Invocation::Current(); call; call = call->previous) {
                    if (&call->slot == slot.get()) {
                        return;
                    }
                }
                slot->idle.wait(lock, [&] { return slot->running == 0; });
            }

        private:
            friend class Subscriptions;
            explicit Connection(std::shared_ptr<Slot> slot) : slot(std::move(slot)) {}

            std::shared_ptr<Slot> slot;
        };

        Connection Subscribe(Callback callback) {
            auto slot = std::make_shared<Slot>(std::move(callback));
            std::lock_guard<std::mutex> lock(this->mutex);

            /* Subscription changes are infrequent. Share a snapshot so
               delivery needs neither a list copy nor a registry lock during calls. */
            auto next = this->slots
                ? std::make_shared<SlotList>(*this->slots)
                : std::make_shared<SlotList>();
            next->erase(std::remove_if(next->begin(), next->end(),
                [](const std::weak_ptr<Slot>& weak) { return weak.expired(); }), next->end());
            next->push_back(slot);
            this->slots = std::move(next);
            return Connection(std::move(slot));
        }

        void Publish(Args... args) {
            std::shared_ptr<const SlotList> snapshot;
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                snapshot = this->slots;
            }
            if (!snapshot) {
                return;
            }

            for (const auto& weak : *snapshot) {
                if (auto slot = weak.lock()) {
                    {
                        std::lock_guard<std::mutex> lock(slot->mutex);
                        if (!slot->connected) {
                            continue;
                        }
                        ++slot->running;
                    }

                    Invocation invocation(*slot);
                    slot->callback(args...);
                }
            }
        }

    private:
        std::mutex mutex;
        std::shared_ptr<const SlotList> slots;
    };

} }
