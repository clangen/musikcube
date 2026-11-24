#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace musik {
    /* fixed storage for a FIFO queue. callers synchronize concurrent access;
    the queue does not allocate storage or overwrite an unconsumed value. */
    template<typename T, std::size_t Capacity>
    class FixedRingQueue {
        static_assert(Capacity > 0, "a ring queue needs at least one slot");

    public:
        FixedRingQueue() = default;
        FixedRingQueue(const FixedRingQueue&) = delete;
        FixedRingQueue(FixedRingQueue&&) = delete;
        FixedRingQueue& operator=(const FixedRingQueue&) = delete;
        FixedRingQueue& operator=(FixedRingQueue&&) = delete;

        bool Empty() const noexcept {
            return this->size == 0;
        }

        std::size_t Size() const noexcept {
            return this->size;
        }

        /* the returned pointer remains valid until its value is popped. */
        T* Front() noexcept {
            return this->Empty() ? nullptr : &*this->values[this->head];
        }

        const T* Front() const noexcept {
            return this->Empty() ? nullptr : &*this->values[this->head];
        }

        /* a full queue leaves the caller's value untouched. */
        template<typename U>
        bool TryPush(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>) {
            if (this->size == Capacity) {
                return false;
            }
            this->values[(this->head + this->size) % Capacity].emplace(std::forward<U>(value));
            ++this->size;
            return true;
        }

        bool TryPop() noexcept {
            if (this->Empty()) {
                return false;
            }
            this->values[this->head].reset();
            this->head = (this->head + 1) % Capacity;
            --this->size;
            return true;
        }

    private:
        std::array<std::optional<T>, Capacity> values {};
        std::size_t head = 0;
        std::size_t size = 0;
    };
}
