#include <musikcore/subscriptions.h>
#include <musikcore/subscriptions.hpp>

#include "error.h"

using musik::core::api::Subscriptions;
using musik::core::ffi::exception_barrier;

namespace {
    /* One scope per thread for both shared-library and consumer callbacks. */
    thread_local unsigned deliveryDepth = 0;
}

void MC_CALL mc_subscription_delivery_enter(void) { ++deliveryDepth; }
void MC_CALL mc_subscription_delivery_leave(void) { --deliveryDepth; }
int MC_CALL mc_subscription_delivery_active(void) { return deliveryDepth != 0; }

struct mc_event_source {
    Subscriptions<const void*> callbacks;
};

struct mc_subscription {
    Subscriptions<const void*>::Connection connection;
};

mc_result MC_CALL mc_event_source_create(mc_event_source **out_source) {
    if (!out_source || *out_source) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&] {
        *out_source = new mc_event_source;
        return MC_OK;
    });
}

void MC_CALL mc_event_source_destroy(mc_event_source **source) {
    if (source) {
        auto* owned = *source;
        *source = nullptr;
        delete owned;
    }
}

mc_result MC_CALL mc_event_source_subscribe(
    mc_event_source *source, mc_event_fn callback, void *user_data,
    mc_subscription **out_subscription)
{
    if (!source || !callback || !out_subscription || *out_subscription) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&] {
        /* Allocate before registering so failure cannot leave a live callback. */
        std::unique_ptr<mc_subscription> subscription(new mc_subscription);
        subscription->connection = source->callbacks.Subscribe(
            [callback, user_data](const void *event) { callback(user_data, event); });
        *out_subscription = subscription.release();
        return MC_OK;
    });
}

mc_result MC_CALL mc_event_source_publish(mc_event_source *source, const void *event) {
    if (!source) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&] {
        source->callbacks.Publish(event);
        return MC_OK;
    });
}

void MC_CALL mc_subscription_destroy(mc_subscription **subscription) {
    if (subscription) {
        auto* owned = *subscription;
        *subscription = nullptr;
        delete owned;
    }
}
