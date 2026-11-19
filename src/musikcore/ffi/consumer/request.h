#ifndef MUSIKCORE_FFI_CONSUMER_REQUEST_H
#define MUSIKCORE_FFI_CONSUMER_REQUEST_H

#include <musikcore/context.h>
#include <functional>
#include <string>

namespace musik { namespace core { namespace ffi {

    using RequestCompletion = std::function<void(mc_result, void *)>;

    struct TrackQueryLimits {
        size_t default_limit;
        size_t max_limit;
        size_t max_fields;
    };

    const TrackQueryLimits &context_track_query_limits(const mc_context &context);

    /* Work owns its inputs because cancellation can dispatch before the worker returns. */
    mc_result submit_request(mc_context *context, std::function<mc_result()> work,
        RequestCompletion completion, void *user_data, mc_request_id *out_request);

    /* Reserves a request for an external producer. Late replies may outlive the context. */
    mc_result reserve_external_request(mc_context *context, RequestCompletion completion,
        void *user_data, mc_request_id *out_request, std::function<void(mc_result)> &finish);

} } }

#endif
