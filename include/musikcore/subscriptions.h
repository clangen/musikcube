#ifndef MUSIKCORE_SUBSCRIPTIONS_H
#define MUSIKCORE_SUBSCRIPTIONS_H

#include <musikcore/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mc_event_source mc_event_source;
typedef struct mc_subscription mc_subscription;

/* Both pointers are borrowed. The event is valid only during the callback.
   The callback must return normally. */
typedef void (MC_CALL *mc_event_fn)(void *user_data, const void *event);

/* Creates a caller-owned source. The output slot must be initialized to null. */
MC_API mc_result MC_CALL mc_event_source_create(mc_event_source **out_source);

/* Destroys the source and clears the slot. Wait for all source operations to
   finish first. Existing subscriptions can still be destroyed separately. */
MC_API void MC_CALL mc_event_source_destroy(mc_event_source **source);

/* Registers a callback and returns a caller-owned subscription. The output
   slot must be initialized to null. Publish and subscribe may overlap. A
   callback can run before subscribe returns. Wait for subscribe to return
   before accessing the output slot from another thread. */
MC_API mc_result MC_CALL mc_event_source_subscribe(
    mc_event_source *source, mc_event_fn callback, void *user_data,
    mc_subscription **out_subscription);

/* Delivers in registration order on the calling thread. Concurrent publishes
   may invoke the same callback concurrently. The event may be null. */
MC_API mc_result MC_CALL mc_event_source_publish(mc_event_source *source, const void *event);

/* Disconnects and clears the slot. Outside a subscription callback this waits
   for admitted calls, so release any locks they need first. Inside a callback
   delivered by the C or C++ subscription APIs it skips the wait to prevent deadlocks.
   In that case keep user_data alive until all admitted calls have finished.
   Synchronize access to an individual handle slot separately. */
MC_API void MC_CALL mc_subscription_destroy(mc_subscription **subscription);

/* Delivery scope used by the header-only C++ subscription implementation.
   Enter and leave must be paired on the publishing thread. */
MC_API void MC_CALL mc_subscription_delivery_enter(void);
MC_API void MC_CALL mc_subscription_delivery_leave(void);
MC_API int MC_CALL mc_subscription_delivery_active(void);

#ifdef __cplusplus
}
#endif

#endif
