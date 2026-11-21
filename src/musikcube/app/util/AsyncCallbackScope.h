#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace musik { namespace cube {
    /* Prevents old async replies from updating a view after its selection changes.
       For example, search A may finish after the user switches to search B;
       cancellation alone cannot retract a completion already queued for delivery.
       Begin(handler) returns a callback that runs only for the current work;
       starting replacement work or destroying the scope disables older ones.
       It does not keep the view alive, schedule work, or cancel requests.
       Noncopyable and nonmovable; used only on the dispatcher thread.

       Use this scope when the caller owns the workflow, such as guarding a
       dialog callback or chaining a catalog read to an online lookup. Use
       LatestRequest for one replaceable query that should retain its inputs,
       cancel superseded work, and retry transient failures.

       Compose the scope as a member of the view, so it dies with the view:

           class SearchView {
               public:
                   void Search() {
                       StartAsyncWork(this->searchWork.Begin([this] {
                           this->UpdateView();
                       }));
                   }

               private:
                   AsyncCallbackScope searchWork;
           };

       StartAsyncWork and UpdateView represent application code. Use Wrap(handler)
       for another callback in the same workflow, or Invalidate() to abandon it.
       Use void C++ completions with owning result values. Raw C payload cleanup
       must not depend on the guarded handler being called. */
    class AsyncCallbackScope {
        public:
            /* Pass validity to another UI component when a wrapped handler is not
               enough. Copyable and movable; does not keep the scope owner alive.
               Prefer Begin(handler) or Wrap(handler) for ordinary completions. */
            struct Token {
                    bool IsCurrent() const {
                        const auto current = this->state.lock();
                        return current && *current == this->version;
                    }

                    std::weak_ptr<const std::size_t> state;
                    std::size_t version;
            };

            AsyncCallbackScope() = default;
            AsyncCallbackScope(const AsyncCallbackScope&) = delete;
            AsyncCallbackScope(AsyncCallbackScope&&) = delete;

            AsyncCallbackScope& operator=(const AsyncCallbackScope&) = delete;
            AsyncCallbackScope& operator=(AsyncCallbackScope&&) = delete;

            /* Start replacement work and check validity before invoking its handler. */
            template<typename Completion>
            auto Begin(Completion completion) {
                this->Invalidate();
                return this->Wrap(std::move(completion));
            }

            /* Attach another completion to the current workflow. The returned
               callback owns the handler, but not this scope or its enclosing view. */
            template<typename Completion>
            auto Wrap(Completion completion) const {
                return [token = this->CurrentToken(), completion = std::move(completion)](auto&&... args) mutable {
                    static_assert(std::is_void_v<std::invoke_result_t<Completion&, decltype(args)...>>,
                        "Scoped callbacks must return void");
                    if (token.IsCurrent()) {
                        std::invoke(completion, std::forward<decltype(args)>(args)...);
                    }
                };
            }

            /* Start replacement work: one call invalidates old callbacks and
               captures the validity token for the new operation. */
            Token Begin() {
                this->Invalidate();
                return this->CurrentToken();
            }

            /* Return the current workflow's token without starting new work
               or invalidating existing callbacks. */
            Token CurrentToken() const { return { this->state, *this->state }; }

            /* Discard pending callbacks when clearing a view without starting a replacement. */
            void Invalidate() { ++*this->state; }

        private:
            std::shared_ptr<std::size_t> state = std::make_shared<std::size_t>(0);
    };
} }
