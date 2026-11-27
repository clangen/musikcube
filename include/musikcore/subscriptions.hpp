#pragma once

#include <musikcore/subscriptions.h>
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace musik { namespace core { namespace api {

    /* Owns a typed list of callbacks. Neither copyable nor movable. Delivery
       runs in registration order on the publisher thread. Concurrent publishes
       may overlap. An exception from a callback propagates and ends that
       delivery. Keep the source alive through all Publish and Subscribe calls. */
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
            explicit Invocation(Slot& slot) : slot(slot) {
                mc_subscription_delivery_enter();
            }

            ~Invocation() {
                mc_subscription_delivery_leave();
                std::lock_guard<std::mutex> lock(this->slot.mutex);
                if (--this->slot.running == 0) {
                    this->slot.idle.notify_all();
                }
            }

            Slot& slot;
        };

        using SlotList = std::vector<std::weak_ptr<Slot>>;

    public:
        /* Move-only owner. Reset waits for admitted callbacks, so call it
           outside locks those callbacks need. Reset inside a callback delivered
           by the C or C++ subscription APIs skips the wait to prevent cyclic
           waits. An admitted callback may still be running on another thread.
           Synchronize access to the same Connection separately. */
        class Connection {
        public:
            Connection() = default;
            Connection(Connection&& other) noexcept = default;
            Connection(const Connection& other) = delete;

            ~Connection() { this->Reset(); }

            Connection& operator=(Connection&& other) noexcept {
                if (this != &other) {
                    this->Reset();
                    this->slot = std::move(other.slot);
                }
                return *this;
            }
            Connection& operator=(const Connection& other) = delete;

            void Reset() {
                auto slot = std::move(this->slot);
                if (!slot) {
                    return;
                }

                std::unique_lock<std::mutex> lock(slot->mutex);
                slot->connected = false;
                if (slot->running == 0 || mc_subscription_delivery_active()) {
                    return;
                }
                slot->idle.wait(lock, [&] { return slot->running == 0; });
            }

        private:
            friend class Subscriptions;
            explicit Connection(std::shared_ptr<Slot> slot) : slot(std::move(slot)) {}

            std::shared_ptr<Slot> slot;
        };

        Subscriptions() = default;
        Subscriptions(const Subscriptions& other) = delete;
        Subscriptions(Subscriptions&& other) = delete;

        Subscriptions& operator=(const Subscriptions& other) = delete;
        Subscriptions& operator=(Subscriptions&& other) = delete;

        [[nodiscard]] Connection Subscribe(Callback callback) {
            if (!callback) {
                return {};
            }

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

} } }
