#pragma once

#include <musikcore/library.h>
#include <musikcore/subscriptions.hpp>
#include <musikcore/library/query/LibraryQueryScope.h>
#include <memory>
#include <mutex>
#include "request.h"

/* mutable selection belongs to the instance; queries retain immutable values. */
class LibraryScopeState {
public:
    using Value = musik::core::library::query::LibraryQueryScope;
    using RequestConnection = musik::core::api::Subscriptions<>::Connection;

    ~LibraryScopeState() { mc_event_source_destroy(&this->events); }

    std::shared_ptr<const Value> Capture() const {
        std::lock_guard lock(this->mutex);
        return this->current;
    }

    bool IsCurrent(const std::shared_ptr<const Value>& snapshot) const {
        std::lock_guard lock(this->mutex);
        return snapshot == this->current;
    }

    /* this check is the completion admission point, ordered with replacement. */
    bool AdmitCompletion(const std::shared_ptr<const Value>& snapshot) const { return this->IsCurrent(snapshot); }

    bool Replace(Value value) {
        auto replacement = std::make_shared<const Value>(std::move(value));
        std::lock_guard lock(this->mutex);
        if (replacement->SourceIds() == this->current->SourceIds()) {
            return false;
        }
        this->current = std::move(replacement);
        return true;
    }

    mc_result Subscribe(mc_event_fn callback, void* data, mc_subscription** out) {
        std::lock_guard lock(this->mutex);
        if (!this->events) {
            const auto result = mc_event_source_create(&this->events);
            if (result != MC_OK) {
                return result;
            }
        }
        return mc_event_source_subscribe(this->events, callback, data, out);
    }

    RequestConnection SubscribeRequests(std::function<void()> callback) {
        return this->requests.Subscribe(std::move(callback));
    }

    void Notify(const mc_library_scope_change& event) {
        mc_event_source* source;
        {
            std::lock_guard lock(this->mutex);
            source = this->events;
        }
        this->requests.Publish();
        if (source) {
            mc_event_source_publish(source, &event);
        }
    }

private:
    mutable std::mutex mutex;
    std::shared_ptr<const Value> current = std::make_shared<const Value>();
    mc_event_source* events = nullptr;
    musik::core::api::Subscriptions<> requests;
};

/* keeps cancellation connected until delivery. callbacks acquire shared ownership
before touching request state, including during reentrant disconnect. */
class ScopedLibraryRequest {
public:
    static std::shared_ptr<ScopedLibraryRequest> Create(
        std::shared_ptr<LibraryScopeState> state, std::shared_ptr<const LibraryScopeState::Value> snapshot)
    {
        auto request =
            std::shared_ptr<ScopedLibraryRequest>(new ScopedLibraryRequest(std::move(state), std::move(snapshot)));
        request->subscription =
            request->state->SubscribeRequests([weak = std::weak_ptr<ScopedLibraryRequest>(request)] {
                if (const auto self = weak.lock()) {
                    self->Changed();
                }
            });
        return request;
    }

    void Bind(mc_context* context, mc_request_id request) {
        try {
            auto cancel = std::make_shared<const std::function<void()>>(
                musik::core::ffi::make_request_canceller(context, request));
            {
                std::lock_guard lock(this->mutex);
                this->cancel = std::move(cancel);
            }
            this->Changed();
        }
        catch (const std::bad_alloc&) {
            /* admission already succeeded. finish through cancellation rather
            than reporting rejection while a callback is still pending. */
            mc_request_cancel(context, request);
        }
    }

private:
    ScopedLibraryRequest(
        std::shared_ptr<LibraryScopeState> state, std::shared_ptr<const LibraryScopeState::Value> snapshot)
    : state(std::move(state))
    , snapshot(std::move(snapshot))
    {
    }

    void Changed() {
        if (this->state->IsCurrent(this->snapshot)) {
            return;
        }
        std::shared_ptr<const std::function<void()>> cancel;
        {
            std::lock_guard lock(this->mutex);
            cancel = this->cancel;
        }
        if (cancel) {
            (*cancel)();
        }
    }

    std::shared_ptr<LibraryScopeState> state;
    std::shared_ptr<const LibraryScopeState::Value> snapshot;
    std::mutex mutex;
    std::shared_ptr<const std::function<void()>> cancel;
    LibraryScopeState::RequestConnection subscription;
};

struct mc_library_scope_snapshot {
    explicit mc_library_scope_snapshot(std::shared_ptr<const LibraryScopeState::Value> value)
    : value(std::move(value)) {
        const auto& ids = this->value->SourceIds();
        this->sourceFilter.ids = ids && !ids->empty() ? ids->data() : nullptr;
        this->sourceFilter.count = ids ? ids->size() : 0;
        this->view.sources = ids ? &this->sourceFilter : nullptr;
    }

    std::shared_ptr<const LibraryScopeState::Value> value;
    mc_library_source_filter sourceFilter = MC_LIBRARY_SOURCE_FILTER_DEFAULT;
    mc_library_scope view = MC_LIBRARY_SCOPE_DEFAULT;
};
