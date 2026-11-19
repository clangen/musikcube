#pragma once

#include <cstddef>
#include <memory>

namespace musik { namespace cube {
    /* Noncopyable and nonmovable. Used only on the dispatcher thread. */
    class RequestGeneration {
    public:
        /* Copyable and movable. A ticket does not extend the owner's lifetime. */
        struct Ticket {
            std::weak_ptr<const std::size_t> state;
            std::size_t version;

            bool IsCurrent() const {
                const auto current = this->state.lock();
                return current && *current == this->version;
            }
        };

        RequestGeneration() = default;
        RequestGeneration(const RequestGeneration&) = delete;
        RequestGeneration& operator=(const RequestGeneration&) = delete;

        Ticket Capture() const { return { this->state, *this->state }; }
        void Invalidate() { ++*this->state; }

    private:
        std::shared_ptr<std::size_t> state = std::make_shared<std::size_t>(0);
    };
} }
