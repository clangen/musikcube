#pragma once

#include <common/DeleteDefaults.h>
#include <thread>

namespace musik { namespace core { namespace ffi {

    /* detects lifecycle reentry into one owner while its code is executing.
    workers never touch the owner-only flag; the engine must stop/join them
    before teardown. this is not worker tracking or lifetime protection.
    state and scopes cannot be copied or moved. */
    class PluginCallState {
    public:
        /* preserve outer calls through nesting and exception unwinding.
        the state outlives every scope, including worker-side scopes. */
        class Scope {
        public:
            MUSIK_DELETE_COPY_AND_MOVE(Scope);

            explicit Scope(PluginCallState& state) noexcept
            : state(state.IsOwnerThread() ? &state : nullptr)
            , previous(this->state && this->state->active) {
                if (this->state) {
                    this->state->active = true;
                }
            }

            ~Scope() {
                if (this->state) {
                    this->state->active = this->previous;
                }
            }

        private:
            PluginCallState* const state;
            const bool previous;
        };

        MUSIK_DELETE_COPY_AND_MOVE(PluginCallState);

        PluginCallState() = default;

        bool IsOwnerThread() const noexcept { return std::this_thread::get_id() == this->owner; }

        bool IsActive() const noexcept { return this->IsOwnerThread() && this->active; }

    private:
        const std::thread::id owner = std::this_thread::get_id();
        bool active = false;
    };

} } }
