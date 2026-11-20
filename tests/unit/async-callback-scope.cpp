/* Check callback validity directly: replacements and owner teardown must make
   old tokens unusable even when cancellation cannot retract queued delivery. */

#include <app/util/AsyncCallbackScope.h>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#define CHECK(expression)                                                         \
    do {                                                                          \
        if (!(expression)) {                                                      \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
            return 1;                                                             \
        }                                                                         \
    } while (false)

/* Exercise both guarded handlers and explicit tokens. Replacement/teardown
   must suppress UI work while normal callback argument cleanup still runs. */
int main() {
    using musik::cube::AsyncCallbackScope;
    static_assert(!std::is_copy_constructible<AsyncCallbackScope>::value, "guard owner must stay unique");
    static_assert(!std::is_move_constructible<AsyncCallbackScope>::value, "guard must stay with its view");

    std::optional<AsyncCallbackScope::Token> retained;
    {
        AsyncCallbackScope callbackWork;

        /* Beginning replacement work performs invalidation and capture together. */
        const auto first = callbackWork.Begin();
        CHECK(first.IsCurrent());
        const auto second = callbackWork.Begin();
        CHECK(!first.IsCurrent() && second.IsCurrent());

        /* Several callbacks in one workflow must coexist; reading another
           token must not invalidate its peers as starting replacement work does. */
        auto peer = callbackWork.CurrentToken();
        CHECK(second.IsCurrent() && peer.IsCurrent());
        auto copy = peer;
        auto moved = std::move(peer);
        CHECK(copy.IsCurrent() && moved.IsCurrent() && !peer.IsCurrent());

        /* Clearing a view abandons work without submitting a replacement. */
        callbackWork.Invalidate();
        CHECK(!second.IsCurrent() && !copy.IsCurrent() && !moved.IsCurrent());
        retained = callbackWork.Begin();
        CHECK(retained->IsCurrent());
    }

    /* A token can remain in a queued callback after its view disappears; its
       validity check must fail without dereferencing the former view. */
    CHECK(!retained->IsCurrent());

    /* Begin replaces older handlers; Wrap adds another handler without
       cancelling its peers. Callers should not need their own token check. */
    unsigned calls = 0;
    {
        AsyncCallbackScope callbackWork;
        auto first = callbackWork.Begin([&](unsigned value) { calls += value; });
        auto peer = callbackWork.Wrap([&](unsigned value) { calls += value; });
        first(1);
        peer(2);
        CHECK(calls == 3);

        auto replacement = callbackWork.Begin([&](unsigned value) { calls += value; });
        first(100);
        peer(100);
        replacement(4);
        CHECK(calls == 7);
        callbackWork.Invalidate();
        replacement(100);
        CHECK(calls == 7);
    }

    /* A completion may outlive its scope even when the owner never explicitly
       invalidates it. Capturing the scope strongly would incorrectly run here. */
    std::function<void()> late;
    {
        AsyncCallbackScope callbackWork;
        late = callbackWork.Begin([&] { ++calls; });
    }
    late();
    CHECK(calls == 7);

    /* The wrapper must preserve mutable/move-only handlers and references,
       rather than adding std::function allocation or copying event arguments. */
    {
        AsyncCallbackScope callbackWork;
        auto mutableHandler = callbackWork.Begin([value = std::make_unique<unsigned>(2), &calls]() mutable {
            calls += (*value)++;
        });
        mutableHandler();
        mutableHandler();
        auto movedHandler = std::move(mutableHandler);
        movedHandler();
        CHECK(calls == 16);

        auto referenceHandler = callbackWork.Wrap([](unsigned& value) { ++value; });
        referenceHandler(calls);
        CHECK(calls == 17);
    }

    /* Skipping a handler must not leak an owned result delivered by a C++
       completion signature. That argument is released when delivery returns. */
    unsigned released = 0;
    struct Payload {
            /* Keep a release counter independent of whether the UI handler runs. */
            explicit Payload(unsigned& released)
            : released(released)
            {}

            /* Observe the result's final owner releasing it after skipped delivery. */
            ~Payload() { ++this->released; }

            unsigned& released;
    };
    std::function<void(std::unique_ptr<Payload>)> lateResult;
    {
        AsyncCallbackScope callbackWork;
        lateResult = callbackWork.Begin([&](std::unique_ptr<Payload>) { ++calls; });
        callbackWork.Invalidate();
        lateResult(std::make_unique<Payload>(released));
        CHECK(released == 1 && calls == 17);
    }
    lateResult(std::make_unique<Payload>(released));
    CHECK(released == 2 && calls == 17);

    /* A handler may destroy its own scope and invoke a peer. The validity
       check must not hold the scope state alive while user code executes. */
    {
        auto nestedWork = std::make_unique<AsyncCallbackScope>();
        auto peer = nestedWork->Wrap([&] { ++calls; });
        auto destroyOwner = nestedWork->Wrap([&] {
            nestedWork.reset();
            peer();
        });
        destroyOwner();
        CHECK(!nestedWork && calls == 17);
    }

    /* Valid handlers keep normal exception behavior; invalidated handlers
       must not execute user code merely to discover that it would throw. */
    {
        AsyncCallbackScope callbackWork;
        auto throwing = callbackWork.Begin([] { throw std::runtime_error("handler failure"); });
        bool caught = false;
        try {
            throwing();
        }
        catch (const std::runtime_error&) {
            caught = true;
        }
        CHECK(caught);
        callbackWork.Invalidate();
        throwing();
    }
    return 0;
}
